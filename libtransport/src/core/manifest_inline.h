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

#include <core/manifest.h>
#include <core/manifest_format.h>
#include <hicn/transport/portability/portability.h>

#include <set>

namespace transport {

namespace core {

template <typename Base, typename FormatTraits>
class ManifestInline
    : public Manifest<Base, FormatTraits, ManifestInline<Base, FormatTraits>> {
  using ManifestBase =
      Manifest<Base, FormatTraits, ManifestInline<Base, FormatTraits>>;

  using Hash = typename FormatTraits::Hash;
  using HashType = typename FormatTraits::HashType;
  using Suffix = typename FormatTraits::Suffix;
  using SuffixList = typename FormatTraits::SuffixList;
  using HashEntry = std::pair<auth::CryptoHashType, std::vector<uint8_t>>;

 public:
  ManifestInline() : ManifestBase() {}

  ManifestInline(Packet::Format format, const core::Name &name,
                 std::size_t signature_size = 0)
      : ManifestBase(format, name, signature_size) {}

  template <typename T>
  ManifestInline(T &&base) : ManifestBase(std::forward<T &&>(base)) {}

  template <typename T>
  ManifestInline(T &base) : ManifestBase(base) {}

  static TRANSPORT_ALWAYS_INLINE ManifestInline *createManifest(
      Packet::Format format, const core::Name &manifest_name,
      ManifestVersion version, ManifestType type, bool is_last,
      const Name &base_name, HashType hash_algo, std::size_t signature_size) {
    auto manifest = new ManifestInline(format, manifest_name, signature_size);
    manifest->setVersion(version);
    manifest->setType(type);
    manifest->setHashAlgorithm(hash_algo);
    manifest->setIsLast(is_last);
    manifest->setBaseName(base_name);
    return manifest;
  }

  ManifestInline &encodeImpl() {
    ManifestBase::encoder_.encode();
    return *this;
  }

  ManifestInline &decodeImpl() {
    base_name_ = ManifestBase::decoder_.getBaseName();
    suffix_hash_map_ = ManifestBase::decoder_.getSuffixHashList();

    return *this;
  }

  std::size_t estimateManifestSizeImpl(std::size_t additional_entries = 0) {
    return ManifestBase::encoder_.estimateSerializedLength(additional_entries);
  }

  ManifestInline &setBaseName(const Name &name) {
    base_name_ = name;
    ManifestBase::encoder_.setBaseName(base_name_);
    return *this;
  }

  const Name &getBaseName() { return base_name_; }

  ManifestInline &addSuffixHash(Suffix suffix, const Hash &hash) {
    ManifestBase::encoder_.addSuffixAndHash(suffix, hash);
    return *this;
  }

  // Call this function only after the decode function!
  const SuffixList &getSuffixList() { return suffix_hash_map_; }

  // Convert several manifests into a single map from suffixes to packet hashes.
  // All manifests must have been decoded beforehand.
  static std::unordered_map<Suffix, Hash> getSuffixMap(
      const std::vector<ManifestInline *> &manifests) {
    std::unordered_map<Suffix, Hash> suffix_map;

    for (auto manifest_ptr : manifests) {
      HashType hash_type = manifest_ptr->getHashAlgorithm();
      SuffixList suffix_list = manifest_ptr->getSuffixList();

      for (auto it = suffix_list.begin(); it != suffix_list.end(); ++it) {
        Hash hash(it->second, Hash::getSize(hash_type), hash_type);
        suffix_map[it->first] = hash;
      }
    }

    return suffix_map;
  }

  static std::unordered_map<Suffix, Hash> getSuffixMap(
      ManifestInline *manifest) {
    return getSuffixMap(std::vector<ManifestInline *>{manifest});
  }

 private:
  core::Name base_name_;
  SuffixList suffix_hash_map_;
};

}  // namespace core
}  // namespace transport
