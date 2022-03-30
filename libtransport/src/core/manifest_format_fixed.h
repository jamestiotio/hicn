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

#include <core/manifest_format.h>
#include <hicn/transport/core/packet.h>

#include <string>

namespace transport {

namespace core {

// Manifest Metadata:
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |Version|  Type | Transport Type| Hash Algorithm|L|   Reserved  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// Manifest Entry Metadata:
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |   Nb entries  |I|                   Reserved                  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                                                               |
// +                                                               +
// |                                                               |
// +                             Prefix                            +
// |                                                               |
// +                                                               +
// |                                                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// Manifest Transport Parameters - Bytestream:
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         Final Segment                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// Manifest Transport Parameters - RTC:
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                                                               |
// +                           Timestamp                           +
// |                                                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                        Production Rate                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                        Current Segment                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |F|                                                             |
// +               Reserved for future parameters                  +
// |                                                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// Manifest Entry:
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         Packet Suffix                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                                                               |
// +                                                               +
// |                                                               |
// +                                                               +
// |                                                               |
// +                                                               +
// |                                                               |
// +                         Packet Digest                         +
// |                                                               |
// +                                                               +
// |                                                               |
// +                                                               +
// |                                                               |
// +                                                               +
// |                                                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class FixedManifestEncoder;
class FixedManifestDecoder;
class Packet;

struct Fixed {
  using Encoder = FixedManifestEncoder;
  using Decoder = FixedManifestDecoder;
  using Hash = auth::CryptoHash;
  using HashType = auth::CryptoHashType;
  using Suffix = uint32_t;
  using SuffixList = std::list<std::pair<uint32_t, uint8_t *>>;
};

const size_t MANIFEST_META_SIZE = 4;
struct __attribute__((__packed__)) ManifestMeta {
  std::uint8_t version : 4;
  std::uint8_t type : 4;
  std::uint8_t transport_type;
  std::uint8_t hash_algorithm;
  std::uint8_t is_last;
};
static_assert(sizeof(ManifestMeta) == MANIFEST_META_SIZE);

const size_t MANIFEST_ENTRY_META_SIZE = 20;
struct __attribute__((__packed__)) ManifestEntryMeta {
  std::uint8_t nb_entries;
  std::uint8_t is_ipv6;
  std::uint16_t unused;
  std::uint32_t prefix[4];
};
static_assert(sizeof(ManifestEntryMeta) == MANIFEST_ENTRY_META_SIZE);

const size_t MANIFEST_PARAMS_BYTESTREAM_SIZE = 4;
struct __attribute__((__packed__)) TransportParamsBytestream {
  std::uint32_t final_segment;
};
static_assert(sizeof(TransportParamsBytestream) ==
              MANIFEST_PARAMS_BYTESTREAM_SIZE);

const size_t MANIFEST_PARAMS_RTC_SIZE = 20;
struct __attribute__((__packed__)) TransportParamsRTC {
  std::uint64_t timestamp;
  std::uint32_t prod_rate;
  std::uint32_t prod_seg;
  std::uint32_t support_fec;
};
static_assert(sizeof(TransportParamsRTC) == MANIFEST_PARAMS_RTC_SIZE);

const size_t MANIFEST_ENTRY_SIZE = 36;
struct __attribute__((__packed__)) ManifestEntry {
  std::uint32_t suffix;
  std::uint32_t hash[8];
};
static_assert(sizeof(ManifestEntry) == MANIFEST_ENTRY_SIZE);

static const constexpr std::uint8_t manifest_version = 1;

class FixedManifestEncoder : public ManifestEncoder<FixedManifestEncoder> {
 public:
  FixedManifestEncoder(Packet &packet, std::size_t signature_size = 0,
                       bool clear = true);

  ~FixedManifestEncoder();

  FixedManifestEncoder &encodeImpl();
  FixedManifestEncoder &clearImpl();
  FixedManifestEncoder &updateImpl();

  // ManifestMeta
  FixedManifestEncoder &setVersionImpl(ManifestVersion version);
  FixedManifestEncoder &setTypeImpl(ManifestType manifest_type);
  FixedManifestEncoder &setHashAlgorithmImpl(Fixed::HashType algorithm);
  FixedManifestEncoder &setIsLastImpl(bool is_last);

  // ManifestEntryMeta
  FixedManifestEncoder &setBaseNameImpl(const core::Name &base_name);

  // TransportParams
  FixedManifestEncoder &setParamsBytestreamImpl(const ParamsBytestream &params);
  FixedManifestEncoder &setParamsRTCImpl(const ParamsRTC &params);

  // ManifestEntry
  FixedManifestEncoder &addSuffixAndHashImpl(uint32_t suffix,
                                             const Fixed::Hash &hash);

  std::size_t estimateSerializedLengthImpl(std::size_t additional_entries = 0);

  static std::size_t manifestHeaderSizeImpl(
      interface::ProductionProtocolAlgorithms transport_type =
          interface::ProductionProtocolAlgorithms::UNKNOWN);
  static std::size_t manifestEntrySizeImpl();

 private:
  Packet &packet_;
  std::size_t max_size_;
  std::size_t signature_size_;
  interface::ProductionProtocolAlgorithms transport_type_;
  bool encoded_;

  // Manifest Header
  ManifestMeta *manifest_meta_;
  ManifestEntryMeta *manifest_entry_meta_;
  TransportParamsBytestream params_bytestream_;
  TransportParamsRTC params_rtc_;

  // Manifest Entries
  std::vector<ManifestEntry> manifest_entries_;
};

class FixedManifestDecoder : public ManifestDecoder<FixedManifestDecoder> {
 public:
  FixedManifestDecoder(Packet &packet);

  ~FixedManifestDecoder();

  void decodeImpl();
  FixedManifestDecoder &clearImpl();

  // ManifestMeta
  ManifestVersion getVersionImpl() const;
  ManifestType getTypeImpl() const;
  interface::ProductionProtocolAlgorithms getTransportTypeImpl() const;
  Fixed::HashType getHashAlgorithmImpl() const;
  bool getIsLastImpl() const;

  // ManifestEntryMeta
  core::Name getBaseNameImpl() const;

  // TransportParams
  ParamsBytestream getParamsBytestreamImpl() const;
  ParamsRTC getParamsRTCImpl() const;

  // ManifestEntry
  typename Fixed::SuffixList getSuffixHashListImpl();

  std::size_t estimateSerializedLengthImpl(
      std::size_t additional_entries = 0) const;

 private:
  Packet &packet_;
  interface::ProductionProtocolAlgorithms transport_type_;
  bool decoded_;

  // Manifest Header
  ManifestMeta *manifest_meta_;
  ManifestEntryMeta *manifest_entry_meta_;
  TransportParamsBytestream *params_bytestream_;
  TransportParamsRTC *params_rtc_;

  // Manifest Entries
  ManifestEntry *manifest_entries_;
};

}  // namespace core

}  // namespace transport
