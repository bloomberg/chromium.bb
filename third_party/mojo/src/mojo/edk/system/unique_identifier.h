// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_UNIQUE_IDENTIFIER_H_
#define MOJO_EDK_SYSTEM_UNIQUE_IDENTIFIER_H_

#include <stdint.h>
#include <string.h>

#include <iosfwd>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace system {

class UniqueIdentifier;

}  // namespace system
}  // namespace mojo

namespace BASE_HASH_NAMESPACE {

// Declare this before |UniqueIdentifier|, so that it can be friended.
template <>
struct hash<mojo::system::UniqueIdentifier>;

}  // BASE_HASH_NAMESPACE

namespace mojo {

namespace embedder {

class PlatformSupport;

}  // namespace embedder

namespace system {

// Declare this before |UniqueIdentifier|, so that it can be friended.
MOJO_SYSTEM_IMPL_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const UniqueIdentifier& unique_identifier);

// |UniqueIdentifier| is a POD class whose value is used to uniquely identify
// things.
class MOJO_SYSTEM_IMPL_EXPORT UniqueIdentifier {
 public:
  // This generates a new identifier. Uniqueness is "guaranteed" (i.e.,
  // probabilistically) for identifiers.
  static UniqueIdentifier Generate(embedder::PlatformSupport* platform_support);

  bool operator==(const UniqueIdentifier& other) const {
    return memcmp(data_, other.data_, sizeof(data_)) == 0;
  }
  bool operator!=(const UniqueIdentifier& other) const {
    return !operator==(other);
  }
  bool operator<(const UniqueIdentifier& other) const {
    return memcmp(data_, other.data_, sizeof(data_)) < 0;
  }

 private:
  friend BASE_HASH_NAMESPACE::hash<mojo::system::UniqueIdentifier>;
  friend MOJO_SYSTEM_IMPL_EXPORT std::ostream& operator<<(
      std::ostream&,
      const UniqueIdentifier&);

  explicit UniqueIdentifier() {}

  char data_[16];

  // Copying and assignment allowed.
};
static_assert(sizeof(UniqueIdentifier) == 16,
              "UniqueIdentifier has wrong size.");
// We want to be able to take any buffer and cast it to a |UniqueIdentifier|.
static_assert(ALIGNOF(UniqueIdentifier) == 1,
              "UniqueIdentifier requires nontrivial alignment.");

}  // namespace system
}  // namespace mojo

namespace BASE_HASH_NAMESPACE {

template <>
struct hash<mojo::system::UniqueIdentifier> {
  size_t operator()(mojo::system::UniqueIdentifier unique_identifier) const {
    return base::HashInts64(
        *reinterpret_cast<uint64_t*>(unique_identifier.data_),
        *reinterpret_cast<uint64_t*>(unique_identifier.data_ +
                                     sizeof(uint64_t)));
  }
};

}  // BASE_HASH_NAMESPACE

#endif  // MOJO_EDK_SYSTEM_UNIQUE_IDENTIFIER_H_
