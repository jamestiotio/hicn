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

#include <implementation/tls_rtc_socket_producer.h>
#include <interfaces/tls_rtc_socket_producer.h>

namespace transport {
namespace interface {

TLSRTCProducerSocket::TLSRTCProducerSocket(
    implementation::TLSRTCProducerSocket *implementation) {
  socket_ =
      std::unique_ptr<implementation::TLSRTCProducerSocket>(implementation);
}

TLSRTCProducerSocket::~TLSRTCProducerSocket() { socket_.release(); }

}  // namespace interface
}  // namespace transport
