/*
 * Copyright (c) 2017-2019 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <math.h>
#include <random>

#include <hicn/transport/interfaces/socket_consumer.h>
#include <hicn/transport/protocols/rtc.h>

namespace transport {

namespace protocol {

using namespace interface;

RTCTransportProtocol::RTCTransportProtocol(
    interface::ConsumerSocket *icnet_socket)
    : TransportProtocol(icnet_socket),
      inflightInterests_(1 << default_values::log_2_default_buffer_size),
      modMask_((1 << default_values::log_2_default_buffer_size) - 1) {
  icnet_socket->getSocketOption(PORTAL, portal_);
  nack_timer_ = std::make_unique<asio::steady_timer>(portal_->getIoService());
  rtx_timer_ = std::make_unique<asio::steady_timer>(portal_->getIoService());
  probe_timer_ = std::make_unique<asio::steady_timer>(portal_->getIoService());
  nack_timer_used_ = false;
  reset();
}

RTCTransportProtocol::~RTCTransportProtocol() {
  if (is_running_) {
    stop();
  }
}

int RTCTransportProtocol::start() {
  probeRtt();
  return TransportProtocol::start();
}

void RTCTransportProtocol::stop() {
  if (!is_running_) return;

  is_running_ = false;
  portal_->stopEventsLoop();
}

void RTCTransportProtocol::resume() {
  if (is_running_) return;

  is_running_ = true;

  lastRoundBegin_ = std::chrono::steady_clock::now();
  inflightInterestsCount_ = 0;

  probeRtt();
  scheduleNextInterests();

  portal_->runEventsLoop();

  is_running_ = false;
}

// private
void RTCTransportProtocol::reset() {
  portal_->setConsumerCallback(this);
  // controller var
  lastRoundBegin_ = std::chrono::steady_clock::now();
  currentState_ = HICN_RTC_SYNC_STATE;

  // cwin var
  currentCWin_ = HICN_INITIAL_CWIN;
  maxCWin_ = HICN_INITIAL_CWIN_MAX;

  // names/packets var
  actualSegment_ = 0;
  inflightInterestsCount_ = 0;
  interestRetransmissions_.clear();
  lastSegNacked_ = 0;
  lastReceived_ = 0;
  highestReceived_ = 0;
  firstSequenceInRound_ = 0;

  nack_timer_used_ = false;
  rtx_timer_used_ = false;
  for(int i = 0; i < (1 << default_values::log_2_default_buffer_size); i++){
    inflightInterests_[i] = {0};
  }

  // stats
  receivedBytes_ = 0;
  sentInterest_ = 0;
  receivedData_ = 0;
  packetLost_ = 0;
  lossRecovered_ = 0;
  avgPacketSize_ = HICN_INIT_PACKET_SIZE;
  gotNack_ = false;
  gotFutureNack_ = 0;
  roundsWithoutNacks_ = 0;
  pathTable_.clear();

  // CC var
  estimatedBw_ = 0.0;
  lossRate_ = 0.0;
  queuingDelay_ = 0.0;
  protocolState_ = HICN_RTC_NORMAL_STATE;

  producerPathLabels_[0] = 0;
  producerPathLabels_[1] = 0;
  initied = false;

  socket_->setSocketOption(GeneralTransportOptions::INTEREST_LIFETIME,
                           (uint32_t)HICN_RTC_INTEREST_LIFETIME);
  // XXX this should be done by the application
}

uint32_t max(uint32_t a, uint32_t b) {
  if (a > b)
    return a;
  else
    return b;
}

uint32_t min(uint32_t a, uint32_t b) {
  if (a < b)
    return a;
  else
    return b;
}

void RTCTransportProtocol::checkRound() {
  uint32_t duration =
      (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - lastRoundBegin_)
          .count();
  if (duration >= HICN_ROUND_LEN) {
    lastRoundBegin_ = std::chrono::steady_clock::now();
    updateStats(duration);  // update stats and window
  }
}

void RTCTransportProtocol::updateDelayStats(
    const ContentObject &content_object) {
  uint32_t segmentNumber = content_object.getName().getSuffix();
  uint32_t pkt = segmentNumber & modMask_;

  if (inflightInterests_[pkt].state != sent_) return;

  if (interestRetransmissions_.find(segmentNumber) !=
      interestRetransmissions_.end())
    // this packet was rtx at least once
    return;

  uint32_t pathLabel = content_object.getPathLabel();

  if (pathTable_.find(pathLabel) == pathTable_.end()) {
    // found a new path
    std::shared_ptr<RTCDataPath> newPath = std::make_shared<RTCDataPath>();
    pathTable_[pathLabel] = newPath;
  }

  // RTT measurements are useful both from NACKs and data packets
  uint64_t RTT = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch())
                     .count() -
                 inflightInterests_[pkt].transmissionTime;

  pathTable_[pathLabel]->insertRttSample(RTT);
  auto payload = content_object.getPayload();

  // we collect OWD only for datapackets
  if (payload->length() != HICN_NACK_HEADER_SIZE) {
    uint64_t *senderTimeStamp = (uint64_t *)payload->data();

    int64_t OWD = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count() -
                  *senderTimeStamp;

    pathTable_[pathLabel]->insertOwdSample(OWD);
    pathTable_[pathLabel]->computeInterArrivalGap(segmentNumber);
  }else{
    pathTable_[pathLabel]->receivedNack();
  }
}

void RTCTransportProtocol::updateStats(uint32_t round_duration) {
  if(pathTable_.empty())
    return;

  if (receivedBytes_ != 0) {
    double bytesPerSec =
        (double)(receivedBytes_ *
                 ((double)HICN_MILLI_IN_A_SEC / (double)round_duration));
    estimatedBw_ = (estimatedBw_ * HICN_ESTIMATED_BW_ALPHA) +
                   ((1 - HICN_ESTIMATED_BW_ALPHA) * bytesPerSec);
  }

  uint64_t minRtt = UINT_MAX;
  uint64_t maxRtt = 0;

  for (auto it = pathTable_.begin(); it != pathTable_.end(); it++) {
    it->second->roundEnd();
    if(it->second->isActive()){
      if(it->second->getMinRtt() < minRtt){
        minRtt = it->second->getMinRtt();
        producerPathLabels_[0] = it->first;
      }
      if(it->second->getMinRtt() > maxRtt){
        maxRtt = it->second->getMinRtt();
        producerPathLabels_[1] = it->first;
      }
    }
  }

  if(pathTable_.find(producerPathLabels_[0]) == pathTable_.end() ||
      pathTable_.find(producerPathLabels_[1]) == pathTable_.end())
        return; //this should not happen

  //as a queuing delay we keep the lowest one among the two paths
  //if one path is congested the forwarder should decide to do not
  //use it soon so it does not make sens to inform the application
  //that maybe we have a problem
  if(pathTable_[producerPathLabels_[0]]->getQueuingDealy() <
      pathTable_[producerPathLabels_[1]]->getQueuingDealy())
      queuingDelay_ = pathTable_[producerPathLabels_[0]]->getQueuingDealy();
  else
      queuingDelay_ = pathTable_[producerPathLabels_[1]]->getQueuingDealy();

  if (sentInterest_ != 0 && currentState_ == HICN_RTC_NORMAL_STATE) {
    uint32_t numberTheoricallyReceivedPackets_ = highestReceived_ - firstSequenceInRound_;
    double lossRate = 0;
    if(numberTheoricallyReceivedPackets_ != 0)
      lossRate = (double)((double)(packetLost_ - lossRecovered_) / (double)numberTheoricallyReceivedPackets_);

    if(lossRate < 0)
      lossRate = 0;

    if(initied){
      lossRate_ = lossRate_ * HICN_ESTIMATED_LOSSES_ALPHA +
                (lossRate * (1 - HICN_ESTIMATED_LOSSES_ALPHA));
    }else {
      lossRate_ =lossRate;
      initied = true;
    }
  }

  if (avgPacketSize_ == 0) avgPacketSize_ = HICN_INIT_PACKET_SIZE;

  //for the BDP we use the max rtt, so that we calibrate the window on the
  //RTT of the slowest path. In this way we are sure that the window will
  //never be too small
  uint32_t BDP = (uint32_t)ceil(
      (estimatedBw_ * (double)((double) pathTable_[producerPathLabels_[1]]->getMinRtt() /
                        (double)HICN_MILLI_IN_A_SEC) *
                        HICN_BANDWIDTH_SLACK_FACTOR) /
                        avgPacketSize_);
  uint32_t BW = (uint32_t)ceil(estimatedBw_);
  computeMaxWindow(BW, BDP);

  ConsumerTimerCallback *stats_callback = nullptr;
  socket_->getSocketOption(ConsumerCallbacksOptions::STATS_SUMMARY,
                           &stats_callback);
  if (*stats_callback != VOID_HANDLER) {
    //Send the stats to the app
    stats_.updateQueuingDelay(queuingDelay_);
    stats_.updateLossRatio(lossRate_);
    (*stats_callback)(*socket_, stats_);
  }

  // bound also by interest lifitime* production rate
  if (!gotNack_) {
    roundsWithoutNacks_++;
    if (currentState_ == HICN_RTC_SYNC_STATE &&
        roundsWithoutNacks_ >= HICN_ROUNDS_IN_SYNC_BEFORE_SWITCH) {
      currentState_ = HICN_RTC_NORMAL_STATE;
    }
  } else {
    roundsWithoutNacks_ = 0;
  }

  updateCCState();
  updateWindow();

  // in any case we reset all the counters

  gotNack_ = false;
  gotFutureNack_ = 0;
  receivedBytes_ = 0;
  sentInterest_ = 0;
  receivedData_ = 0;
  packetLost_ = 0;
  lossRecovered_ = 0;
  firstSequenceInRound_ = highestReceived_;
}

void RTCTransportProtocol::updateCCState() {
  // TODO
}

void RTCTransportProtocol::computeMaxWindow(uint32_t productionRate,
                                            uint32_t BDPWin) {
  if (productionRate ==
      0)  // we have no info about the producer, keep the previous maxCWin
    return;

  uint32_t interestLifetime = default_values::interest_lifetime;
  socket_->getSocketOption(GeneralTransportOptions::INTEREST_LIFETIME,
                           interestLifetime);
  uint32_t maxWaintingInterest = (uint32_t)ceil(
      (productionRate / avgPacketSize_) *
      (double)((double)(interestLifetime *
                        HICN_INTEREST_LIFETIME_REDUCTION_FACTOR) /
               (double)HICN_MILLI_IN_A_SEC));

  if (currentState_ == HICN_RTC_SYNC_STATE) {
    // in this case we do not limit the window with the BDP, beacuse most
    // likely it is wrong
    maxCWin_ = maxWaintingInterest;
    return;
  }

  // currentState = RTC_NORMAL_STATE
  if (BDPWin != 0) {
    maxCWin_ =
        (uint32_t)ceil((double)BDPWin + ((double)BDPWin / 10.0));  // BDP + 10%
  } else {
    maxCWin_ = min(maxWaintingInterest, maxCWin_);
  }
}

void RTCTransportProtocol::updateWindow() {
  if (currentState_ == HICN_RTC_SYNC_STATE) return;

  if (currentCWin_ < maxCWin_ * 0.7) {
    currentCWin_ =
        min(maxCWin_, (uint32_t)(currentCWin_ * HICN_WIN_INCREASE_FACTOR));
  } else if (currentCWin_ > maxCWin_) {
    currentCWin_ =
        max((uint32_t)(currentCWin_ * HICN_WIN_DECREASE_FACTOR), HICN_MIN_CWIN);
  }
}

void RTCTransportProtocol::decreaseWindow() {
  // this is used only in SYNC mode
  if (currentState_ == HICN_RTC_NORMAL_STATE) return;

  if (gotFutureNack_ == 1)
    currentCWin_ = min((currentCWin_ - 1),
                       (uint32_t)ceil((double)maxCWin_ * 0.66));  // 2/3
  else
    currentCWin_--;

  currentCWin_ = max(currentCWin_, HICN_MIN_CWIN);
}

void RTCTransportProtocol::increaseWindow() {
  // this is used only in SYNC mode
  if (currentState_ == HICN_RTC_NORMAL_STATE) return;

  // we need to be carefull to do not increase the window to much
  if (currentCWin_ < ((double)maxCWin_ * 0.5)) {
    currentCWin_ = currentCWin_ + 1;  // exponential
  } else {
    currentCWin_ = min(
        maxCWin_,
        (uint32_t)ceil(currentCWin_ + (1.0 / (double)currentCWin_)));  // linear
  }
}

void RTCTransportProtocol::probeRtt(){
  time_sent_probe_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();

  Name *interest_name = nullptr;
  socket_->getSocketOption(GeneralTransportOptions::NETWORK_NAME,
                                 &interest_name);
  //get a random numbe in the probe seq range
  std::default_random_engine eng((std::random_device())());
  std::uniform_int_distribution<uint32_t> idis(
        HICN_MIN_PROBE_SEQ, HICN_MAX_PROBE_SEQ);
  probe_seq_number_ = idis(eng);
  interest_name->setSuffix(probe_seq_number_);

  //we considere the probe as a rtx so that we do not incresea inFlightInt
  received_probe_ = false;
  sendInterest(interest_name, true);

  probe_timer_->expires_from_now(std::chrono::milliseconds(1000));
  probe_timer_->async_wait([this](std::error_code ec) {
    if (ec) return;
    probeRtt();
  });
}


void RTCTransportProtocol::sendInterest(Name *interest_name, bool rtx) {
  auto interest = getPacket();
  interest->setName(*interest_name);

  uint32_t interestLifetime = default_values::interest_lifetime;
  socket_->getSocketOption(GeneralTransportOptions::INTEREST_LIFETIME,
                           interestLifetime);
  interest->setLifetime(uint32_t(interestLifetime));

  ConsumerInterestCallback *on_interest_output = nullptr;

  socket_->getSocketOption(ConsumerCallbacksOptions::INTEREST_OUTPUT,
                           &on_interest_output);

  if (*on_interest_output != VOID_HANDLER) {
    (*on_interest_output)(*socket_, *interest);
  }

  if (TRANSPORT_EXPECT_FALSE(!is_running_)) {
    return;
  }

  portal_->sendInterest(std::move(interest));

  sentInterest_++;

  if (!rtx) {
    inflightInterestsCount_++;
  }
}

void RTCTransportProtocol::scheduleNextInterests() {
  checkRound();
  if (!is_running_) return;

  while (inflightInterestsCount_ < currentCWin_) {
    Name *interest_name = nullptr;
    socket_->getSocketOption(GeneralTransportOptions::NETWORK_NAME,
                             &interest_name);

    // we send the packet only if it is not pending yet
    interest_name->setSuffix(actualSegment_);
    if (portal_->interestIsPending(*interest_name)) {
      actualSegment_ = (actualSegment_ + 1) % HICN_MIN_PROBE_SEQ;
      continue;
    }

    uint32_t pkt = actualSegment_ & modMask_;
    // if we already reacevied the content we don't ask it again
    if (inflightInterests_[pkt].state == received_ &&
        inflightInterests_[pkt].sequence == actualSegment_) {
      actualSegment_ = (actualSegment_ + 1) % HICN_MIN_PROBE_SEQ;
      continue;
    }

    //same if the packet is lost
    if (inflightInterests_[pkt].state == lost_ &&
        inflightInterests_[pkt].sequence == actualSegment_){
      actualSegment_ = (actualSegment_ + 1) % HICN_MIN_PROBE_SEQ;
      continue;
    }

    inflightInterests_[pkt].transmissionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();

    //here the packet can be in any state except for lost or recevied
    inflightInterests_[pkt].state = sent_;
    inflightInterests_[pkt].sequence = actualSegment_;
    actualSegment_ = (actualSegment_ + 1) % HICN_MIN_PROBE_SEQ;

    sendInterest(interest_name, false);
    checkRound();
  }
}

void RTCTransportProtocol::addRetransmissions(uint32_t val) {
  // add only val in the rtx list
  addRetransmissions(val, val + 1);
}

void RTCTransportProtocol::addRetransmissions(uint32_t start, uint32_t stop) {
  uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

  for (uint32_t i = start; i < stop; i++) {
    auto it = interestRetransmissions_.find(i);
    if (it == interestRetransmissions_.end()) {
      if (lastSegNacked_ <= i) {
        // i must be larger than the last past nack received
        packetLost_++;
        interestRetransmissions_[i] = 0;
        uint32_t pkt = i & modMask_;
        //we reset the transmission time setting to now, so that rtx will
        //happne in one RTT on waint one inter arrival gap
        inflightInterests_[pkt].transmissionTime = now;
      }
    }  // if the retransmission is already there the rtx timer will
       // take care of it
  }

  if(!rtx_timer_used_)
    checkRtx();
}

void RTCTransportProtocol::retransmit() {
  auto it = interestRetransmissions_.begin();

  // cut len to max HICN_MAX_RTX_SIZE
  // since we use a map, the smaller (and so the older) sequence number are at
  // the beginnin of the map
  while (interestRetransmissions_.size() > HICN_MAX_RTX_SIZE) {
    it = interestRetransmissions_.erase(it);
  }

  it = interestRetransmissions_.begin();

  while (it != interestRetransmissions_.end()) {
    uint32_t pkt = it->first & modMask_;

    if (inflightInterests_[pkt].sequence != it->first) {
      // this packet is not anymore in the inflight buffer, erase it
      it = interestRetransmissions_.erase(it);
      continue;
    }

    // we retransmitted the packet too many times
    if (it->second >= HICN_MAX_RTX) {
      it = interestRetransmissions_.erase(it);
      continue;
    }

    // this packet is too old
    if ((lastReceived_ > it->first) &&
        (lastReceived_ - it->first) > HICN_MAX_RTX_MAX_AGE) {
      it = interestRetransmissions_.erase(it);
      continue;
    }


    uint64_t sent_time = inflightInterests_[pkt].transmissionTime;
    uint64_t rtx_time = sent_time;

    if(it->second == 0){
      if(pathTable_.find(producerPathLabels_[0]) != pathTable_.end() &&
        pathTable_.find(producerPathLabels_[1]) != pathTable_.end()){
        //first rtx: wait RTTmax - RTTmin + gap
        rtx_time = sent_time + pathTable_[producerPathLabels_[1]]->getMinRtt() -
                  pathTable_[producerPathLabels_[0]]->getMinRtt() +
                  pathTable_[producerPathLabels_[1]]->getInterArrivalGap();
      }
    }else{
      if(pathTable_.find(producerPathLabels_[0]) != pathTable_.end()){
        //second+ rtx: waint min rtt
        rtx_time = sent_time + pathTable_[producerPathLabels_[0]]->getMinRtt();
      }
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

    if(now >= rtx_time){
      inflightInterests_[pkt].transmissionTime = now;
      it->second++;

      Name *interest_name = nullptr;
      socket_->getSocketOption(GeneralTransportOptions::NETWORK_NAME,
                                 &interest_name);
      interest_name->setSuffix(it->first);
      sendInterest(interest_name, true);
    }

    ++it;
  }
}

void RTCTransportProtocol::checkRtx() {
  if(interestRetransmissions_.empty()){
    rtx_timer_used_ = false;
    return;
  }

  //we use the packet intearriva time on the fastest path
  //even if this stats should be the same on both
  auto pathStats = pathTable_.find(producerPathLabels_[0]);
  uint64_t wait = 1;
  if(pathStats != pathTable_.end()){
    wait = floor(pathStats->second->getInterArrivalGap() / 2.0);
    if(wait < 1)
      wait = 1;
  }

  rtx_timer_used_ = true;
  retransmit();
  rtx_timer_->expires_from_now(std::chrono::milliseconds(wait));
  rtx_timer_->async_wait([this](std::error_code ec) {
    if (ec) return;
    checkRtx();
  });
}

void RTCTransportProtocol::onTimeout(Interest::Ptr &&interest) {
  // packetLost_++;

  uint32_t segmentNumber = interest->getName().getSuffix();
  uint32_t pkt = segmentNumber & modMask_;

  if (inflightInterests_[pkt].state == sent_) {
    inflightInterestsCount_--;
  }

  // check how many times we sent this packet
  auto it = interestRetransmissions_.find(segmentNumber);
  if (it != interestRetransmissions_.end() && it->second >= HICN_MAX_RTX) {
    inflightInterests_[pkt].state = lost_;
  }

  if (inflightInterests_[pkt].state == sent_) {
    inflightInterests_[pkt].state = timeout1_;
  } else if (inflightInterests_[pkt].state == timeout1_) {
    inflightInterests_[pkt].state = timeout2_;
  } else if (inflightInterests_[pkt].state == timeout2_) {
    inflightInterests_[pkt].state = lost_;
  }

  if (inflightInterests_[pkt].state == lost_) {
    interestRetransmissions_.erase(segmentNumber);
  } else {
    addRetransmissions(segmentNumber);
  }

  scheduleNextInterests();
}

bool RTCTransportProtocol::checkIfProducerIsActive(
    const ContentObject &content_object) {
  uint32_t *payload = (uint32_t *)content_object.getPayload()->data();
  uint32_t productionSeg = *payload;
  uint32_t productionRate = *(++payload);

  if (productionRate == 0) {
    // the producer socket is not active
    // in this case we consider only the first nack
    if (nack_timer_used_) {
      return false;
    }

    nack_timer_used_ = true;
    // actualSegment_ should be the one in the nack, which will be the next in
    // production
    actualSegment_ = productionSeg;
    // all the rest (win size should not change)
    // we wait a bit before pull the socket again
    nack_timer_->expires_from_now(std::chrono::milliseconds(500));
    nack_timer_->async_wait([this](std::error_code ec) {
      if (ec) return;
      nack_timer_used_ = false;
      scheduleNextInterests();
    });
    return false;
  }
  return true;
}

bool RTCTransportProtocol::onNack(const ContentObject &content_object, bool rtx) {
  uint32_t *payload = (uint32_t *)content_object.getPayload()->data();
  uint32_t productionSeg = *payload;
  uint32_t productionRate = *(++payload);
  uint32_t nackSegment = content_object.getName().getSuffix();

  bool old_nack = false;

  if(!rtx){
    gotNack_ = true;
    // we synch the estimated production rate with the actual one
    estimatedBw_ = (double)productionRate;
  }

  if (productionSeg > nackSegment) {
    // we are asking for stuff produced in the past
    actualSegment_ = max(productionSeg + 1, actualSegment_) % HICN_MIN_PROBE_SEQ;

    if(!rtx) {
      if (currentState_ == HICN_RTC_NORMAL_STATE) {
        currentState_ = HICN_RTC_SYNC_STATE;
      }

      computeMaxWindow(productionRate, 0);
      increaseWindow();
    }

    //we need to remove the rtx for packets with seq number
    //< productionSeg
    for(auto it = interestRetransmissions_.begin(); it !=
          interestRetransmissions_.end();){
      if(it->first < productionSeg)
        it = interestRetransmissions_.erase(it);
      else
        ++it;
    }

    lastSegNacked_ = productionSeg;
    old_nack = true;

  } else if (productionSeg < nackSegment) {
    actualSegment_ = (productionSeg + 1) % HICN_MIN_PROBE_SEQ;

    if(!rtx){
      // we are asking stuff in the future
      gotFutureNack_++;
      computeMaxWindow(productionRate, 0);
      decreaseWindow();

      if (currentState_ == HICN_RTC_SYNC_STATE) {
        currentState_ = HICN_RTC_NORMAL_STATE;
      }
    }
  }  // equal should not happen

  return old_nack;
}

void RTCTransportProtocol::onContentObject(
    Interest::Ptr &&interest, ContentObject::Ptr &&content_object) {
  auto payload = content_object->getPayload();
  uint32_t payload_size = (uint32_t)payload->length();
  uint32_t segmentNumber = content_object->getName().getSuffix();
  uint32_t pkt = segmentNumber & modMask_;
  bool schedule_next_interest = true;

  ConsumerContentObjectCallback *callback_content_object = nullptr;
  socket_->getSocketOption(ConsumerCallbacksOptions::CONTENT_OBJECT_INPUT,
                           &callback_content_object);
  if (*callback_content_object != VOID_HANDLER) {
    (*callback_content_object)(*socket_, *content_object);
  }

  if(segmentNumber == probe_seq_number_){
    if(payload_size == HICN_NACK_HEADER_SIZE){
      if(!received_probe_){
        received_probe_ = true;

        uint32_t pathLabel = content_object->getPathLabel();
        if (pathTable_.find(pathLabel) == pathTable_.end()){
          //if this path does not exists we cannot create a new one so drop
          return;
        }

        //this is the expected probe, update the RTT and drop the packet
        uint64_t RTT = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch())
                     .count() - time_sent_probe_;

        pathTable_[pathLabel]->insertRttSample(RTT);
        pathTable_[pathLabel]->receivedNack();
        return;
      }
    }else{
      //this should never happen
      //don't know what to do, let's try to process it as normal packet
    }
  }

  if (payload_size == HICN_NACK_HEADER_SIZE) {
    schedule_next_interest = checkIfProducerIsActive(*content_object);

    if (inflightInterests_[pkt].state == sent_) {
      inflightInterestsCount_--;
    }

    // if checkIfProducerIsActive returns false, we did all we need to do
    // inside that function, no need to call onNack
    bool old_nack = false;

    if (schedule_next_interest){
      if (interestRetransmissions_.find(segmentNumber) ==
            interestRetransmissions_.end()){
        //this is not a retransmitted packet
        old_nack = onNack(*content_object, false);
        updateDelayStats(*content_object);
      } else {
        old_nack = onNack(*content_object, true);
      }
    }

    //the nacked_ state is used only to avoid to decrease inflightInterestsCount_
    //multiple times. In fact, every time that we receive an event related to an
    //interest (timeout, nacked, content) we cange the state. In this way we are
    //sure that we do not decrease twice the counter
    if(old_nack)
      inflightInterests_[pkt].state = lost_;
    else
      inflightInterests_[pkt].state = nacked_;

  } else {
    avgPacketSize_ = (HICN_ESTIMATED_PACKET_SIZE * avgPacketSize_) +
                     ((1 - HICN_ESTIMATED_PACKET_SIZE) * payload->length());

    if (inflightInterests_[pkt].state == sent_) {
      inflightInterestsCount_--;  // packet sent without timeouts
    }

    if (inflightInterests_[pkt].state == sent_ &&
        interestRetransmissions_.find(segmentNumber) ==
            interestRetransmissions_.end()) {
      // we count only non retransmitted data in order to take into accunt only
      // the transmition rate of the producer
      receivedBytes_ += (uint32_t)(content_object->headerSize() +
                                   content_object->payloadSize());
      updateDelayStats(*content_object);

      addRetransmissions(lastReceived_ + 1, segmentNumber);
      if(segmentNumber > highestReceived_)
        highestReceived_ = segmentNumber;
      // lastReceived_ is updated only for data packets received without RTX
      lastReceived_ = segmentNumber;
    }

    receivedData_++;
    inflightInterests_[pkt].state = received_;

    reassemble(std::move(content_object));
    increaseWindow();
  }

  // in any case we remove the packet from the rtx list
  auto it = interestRetransmissions_.find(segmentNumber);
  if(it != interestRetransmissions_.end())
    lossRecovered_ ++;

  interestRetransmissions_.erase(segmentNumber);

  if (schedule_next_interest) {
    scheduleNextInterests();
  }
}

void RTCTransportProtocol::returnContentToApplication(
    const ContentObject &content_object) {
  // return content to the user
  auto read_buffer = content_object.getPayload();

  read_buffer->trimStart(HICN_TIMESTAMP_SIZE);

  interface::ConsumerSocket::ReadCallback *read_callback = nullptr;
  socket_->getSocketOption(READ_CALLBACK, &read_callback);

  if (read_callback == nullptr) {
    throw errors::RuntimeException(
        "The read callback must be installed in the transport before starting "
        "the content retrieval.");
  }

  if (read_callback->isBufferMovable()) {
    read_callback->readBufferAvailable(
        utils::MemBuf::copyBuffer(read_buffer->data(), read_buffer->length()));
  } else {
    // The buffer will be copied into the application-provided buffer
    uint8_t *buffer;
    std::size_t length;
    std::size_t total_length = read_buffer->length();

    while (read_buffer->length()) {
      buffer = nullptr;
      length = 0;
      read_callback->getReadBuffer(&buffer, &length);

      if (!buffer || !length) {
        throw errors::RuntimeException(
            "Invalid buffer provided by the application.");
      }

      auto to_copy = std::min(read_buffer->length(), length);

      std::memcpy(buffer, read_buffer->data(), to_copy);
      read_buffer->trimStart(to_copy);
    }

    read_callback->readDataAvailable(total_length);
    read_buffer->clear();
  }
}

}  // end namespace protocol

}  // end namespace transport
