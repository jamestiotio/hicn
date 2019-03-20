/*
 * Copyright (c) 2017 Cisco and/or its affiliates.
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

#include "content.h"

namespace icn_httpserver {

Content::Content(asio::streambuf &streambuf)
    : std::istream(&streambuf), streambuf_(streambuf) {}

std::size_t Content::size() { return streambuf_.size(); }

std::string Content::string() {
  std::stringstream ss;
  ss << rdbuf();
  return ss.str();
}

} // end namespace icn_httpserver