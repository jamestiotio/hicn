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

#pragma once

#include <hicn/transport/interfaces/socket_producer.h>
#include <hicn/transport/utils/content_store.h>

#include <atomic>
#include <map>
#include <mutex>

namespace transport {

namespace interface {

class RTCProducerSocket : public ProducerSocket {
 public:
  RTCProducerSocket(asio::io_service &io_service);

  RTCProducerSocket();

  ~RTCProducerSocket();

  void registerPrefix(const Prefix &producer_namespace) override;

  void produce(const uint8_t *buffer, size_t buffer_size) override;

  void onInterest(Interest::Ptr &&interest) override;

 private:
  void sendNack(const Interest &interest, bool isActvie);
  void updateStats(uint32_t packet_size, uint64_t now);

  uint32_t currentSeg_;
  uint32_t prodLabel_;
  uint16_t headerSize_;
  Name flowName_;
  uint32_t producedBytes_;
  uint32_t producedPackets_;
  uint32_t bytesProductionRate_;
  std::atomic<uint32_t> packetsProductionRate_;
  uint32_t perSecondFactor_;
  uint64_t lastStats_;

  uint64_t lastProduced_;
  bool active_;
  utils::SpinLock lock_;
};

}  // namespace interface

}  // end namespace transport
