/*
 * Copyright (c) 2021 Cisco and/or its affiliates.
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

#include <glog/logging.h>
#include <protocols/datagram_reassembly.h>
#include <protocols/rtc/rtc_consts.h>

namespace transport {

namespace protocol {

namespace rtc {

class RtcReassembly : public DatagramReassembly {
 public:
  RtcReassembly(implementation::ConsumerSocket *icn_socket,
                TransportProtocol *transport_protocol);

  void reassemble(core::ContentObject &content_object) override;
  void reassemble(utils::MemBuf &buffer, uint32_t suffix) override;

 private:
  bool is_setup_;
  bool data_aggregation_;
};

}  // namespace rtc
}  // namespace protocol
}  // end namespace transport
