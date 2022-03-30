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

#include <core/manifest_format_fixed.h>
#include <core/manifest_inline.h>
#include <gtest/gtest.h>
#include <hicn/transport/auth/crypto_hash.h>
#include <hicn/transport/auth/signer.h>
#include <hicn/transport/auth/verifier.h>
#include <test/packet_samples.h>

#include <climits>
#include <random>
#include <vector>

namespace transport {

namespace core {

namespace {
// The fixture for testing class Foo.
class ManifestTest : public ::testing::Test {
 protected:
  using ContentObjectManifest = ManifestInline<ContentObject, Fixed>;

  ManifestTest() : name_("b001::123|321"), manifest1_(HF_INET6_TCP_AH, name_) {
    // You can do set-up work for each test here.
  }

  virtual ~ManifestTest() {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

  Name name_;
  ContentObjectManifest manifest1_;

  std::vector<uint8_t> manifest_payload = {
      0x11, 0x11, 0x01, 0x00, 0xb0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xde, 0xad  // , 0x00, 0x00,
                                                            // 0x00, 0x45, 0xa3,
                                                            // 0xd1, 0xf2, 0x2b,
                                                            // 0x94, 0x41, 0x22,
                                                            // 0xc9, 0x00, 0x00,
                                                            // 0x00, 0x44, 0xa3,
                                                            // 0xd1, 0xf2, 0x2b,
                                                            // 0x94, 0x41, 0x22,
                                                            // 0xc8
  };
};

}  // namespace

TEST_F(ManifestTest, MoveConstructor) {
  // Create content object with manifest in payload
  ContentObject co(HF_INET6_TCP_AH, 128);
  co.appendPayload(&manifest_payload[0], manifest_payload.size());
  uint8_t buffer[256];
  co.appendPayload(buffer, 256);

  // Copy packet payload
  uint8_t packet[1500];
  auto length = co.getPayload()->length();
  std::memcpy(packet, co.getPayload()->data(), length);

  // Create manifest
  ContentObjectManifest m(std::move(co));

  // Check manifest payload is exactly the same of content object
  ASSERT_EQ(length, m.getPayload()->length());
  auto ret = std::memcmp(packet, m.getPayload()->data(), length);
  ASSERT_EQ(ret, 0);
}

TEST_F(ManifestTest, SetLastManifest) {
  manifest1_.clear();

  manifest1_.setIsLast(true);
  bool fcn = manifest1_.getIsLast();

  ASSERT_TRUE(fcn == true);
}

TEST_F(ManifestTest, SetManifestType) {
  manifest1_.clear();

  ManifestType type1 = ManifestType::INLINE_MANIFEST;
  ManifestType type2 = ManifestType::FLIC_MANIFEST;

  manifest1_.setType(type1);
  ManifestType type_returned1 = manifest1_.getType();

  manifest1_.clear();

  manifest1_.setType(type2);
  ManifestType type_returned2 = manifest1_.getType();

  ASSERT_EQ(type1, type_returned1);
  ASSERT_EQ(type2, type_returned2);
}

TEST_F(ManifestTest, SetHashAlgorithm) {
  manifest1_.clear();

  auth::CryptoHashType hash1 = auth::CryptoHashType::SHA512;
  auth::CryptoHashType hash2 = auth::CryptoHashType::BLAKE2B512;
  auth::CryptoHashType hash3 = auth::CryptoHashType::SHA256;

  manifest1_.setHashAlgorithm(hash1);
  auto type_returned1 = manifest1_.getHashAlgorithm();

  manifest1_.clear();

  manifest1_.setHashAlgorithm(hash2);
  auto type_returned2 = manifest1_.getHashAlgorithm();

  manifest1_.clear();

  manifest1_.setHashAlgorithm(hash3);
  auto type_returned3 = manifest1_.getHashAlgorithm();

  ASSERT_EQ(hash1, type_returned1);
  ASSERT_EQ(hash2, type_returned2);
  ASSERT_EQ(hash3, type_returned3);
}

TEST_F(ManifestTest, setParamsBytestream) {
  manifest1_.clear();

  ParamsBytestream params{
      .final_segment = 1,
  };

  manifest1_.setParamsBytestream(params);
  manifest1_.encode();

  ContentObjectManifest manifest(manifest1_);
  manifest.decode();

  ASSERT_EQ(interface::ProductionProtocolAlgorithms::BYTE_STREAM,
            manifest.getTransportType());
  ASSERT_EQ(params, manifest.getParamsBytestream());
}

TEST_F(ManifestTest, SetParamsRTC) {
  manifest1_.clear();

  ParamsRTC params{
      .timestamp = 1,
      .prod_rate = 2,
      .prod_seg = 3,
      .support_fec = 1,
  };

  manifest1_.setParamsRTC(params);
  manifest1_.encode();

  ContentObjectManifest manifest(manifest1_);
  manifest.decode();

  ASSERT_EQ(interface::ProductionProtocolAlgorithms::RTC_PROD,
            manifest.getTransportType());
  ASSERT_EQ(params, manifest.getParamsRTC());
}

TEST_F(ManifestTest, SignManifest) {
  Name name("b001::", 0);
  auto signer = std::make_shared<auth::SymmetricSigner>(
      auth::CryptoSuite::HMAC_SHA256, "hunter2");
  auto verifier = std::make_shared<auth::SymmetricVerifier>("hunter2");
  std::shared_ptr<ContentObjectManifest> manifest;

  // Instantiate Manifest
  manifest.reset(ContentObjectManifest::createManifest(
      HF_INET6_TCP_AH, name, ManifestVersion::VERSION_1,
      ManifestType::INLINE_MANIFEST, false, name, signer->getHashType(),
      signer->getSignatureFieldSize()));

  // Add Manifest entry
  auth::CryptoHash hash(signer->getHashType());
  hash.computeDigest(std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04});
  manifest->addSuffixHash(1, hash);

  // Encode manifest
  manifest->encode();

  // Sign manifest
  signer->signPacket(manifest.get());

  // Check size
  ASSERT_EQ(manifest->payloadSize(), manifest->estimateManifestSize());
  ASSERT_EQ(manifest->length(),
            manifest->headerSize() + manifest->payloadSize());
  ASSERT_EQ(ContentObjectManifest::manifestHeaderSize(
                interface::ProductionProtocolAlgorithms::UNKNOWN),
            manifest->manifestHeaderSize());

  // Verify manifest
  auth::VerificationPolicy policy = verifier->verifyPackets(manifest.get());
  ASSERT_EQ(auth::VerificationPolicy::ACCEPT, policy);
}

TEST_F(ManifestTest, SetBaseName) {
  manifest1_.clear();

  core::Name base_name("b001::dead");
  manifest1_.setBaseName(base_name);
  core::Name ret_name = manifest1_.getBaseName();

  ASSERT_EQ(base_name, ret_name);
}

TEST_F(ManifestTest, SetSuffixList) {
  manifest1_.clear();

  core::Name base_name("b001::dead");

  using random_bytes_engine =
      std::independent_bits_engine<std::default_random_engine, CHAR_BIT,
                                   unsigned char>;
  random_bytes_engine rbe;

  std::default_random_engine eng((std::random_device())());
  std::uniform_int_distribution<uint64_t> idis(
      0, std::numeric_limits<uint32_t>::max());

  auto entries = new std::pair<uint32_t, auth::CryptoHash>[3];
  uint32_t suffixes[3];
  std::vector<unsigned char> data[3];

  for (int i = 0; i < 3; i++) {
    data[i].resize(32);
    std::generate(std::begin(data[i]), std::end(data[i]), std::ref(rbe));
    suffixes[i] = idis(eng);
    entries[i] = std::make_pair(suffixes[i],
                                auth::CryptoHash(data[i].data(), data[i].size(),
                                                 auth::CryptoHashType::SHA256));
    manifest1_.addSuffixHash(entries[i].first, entries[i].second);
  }

  manifest1_.setBaseName(base_name);
  core::Name ret_name = manifest1_.getBaseName();

  ASSERT_EQ(base_name, ret_name);

  delete[] entries;
}

}  // namespace core

}  // namespace transport
