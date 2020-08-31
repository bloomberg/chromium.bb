// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_IP_ADDRESS_H_
#define PLATFORM_BASE_IP_ADDRESS_H_

#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>

#include "platform/base/error.h"

namespace openscreen {

class IPAddress {
 public:
  enum class Version {
    kV4,
    kV6,
  };

  static const IPAddress kV4LoopbackAddress;
  static const IPAddress kV6LoopbackAddress;

  static constexpr size_t kV4Size = 4;
  static constexpr size_t kV6Size = 16;

  IPAddress();

  // |bytes| contains 4 octets for IPv4, or 8 hextets (16 bytes of big-endian
  // shorts) for IPv6.
  IPAddress(Version version, const uint8_t* bytes);

  // IPv4 constructors (IPAddress from 4 octets).
  explicit IPAddress(const std::array<uint8_t, 4>& bytes);
  explicit IPAddress(const uint8_t (&b)[4]);
  IPAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4);

  // IPv6 constructors (IPAddress from 8 hextets).
  explicit IPAddress(const std::array<uint16_t, 8>& hextets);
  explicit IPAddress(const uint16_t (&hextets)[8]);
  IPAddress(uint16_t h1,
            uint16_t h2,
            uint16_t h3,
            uint16_t h4,
            uint16_t h5,
            uint16_t h6,
            uint16_t h7,
            uint16_t h8);

  IPAddress(const IPAddress& o) noexcept;
  IPAddress(IPAddress&& o) noexcept;
  ~IPAddress() = default;

  IPAddress& operator=(const IPAddress& o) noexcept;
  IPAddress& operator=(IPAddress&& o) noexcept;

  bool operator==(const IPAddress& o) const;
  bool operator!=(const IPAddress& o) const;

  bool operator<(const IPAddress& other) const;
  bool operator>(const IPAddress& other) const { return other < *this; }
  bool operator<=(const IPAddress& other) const { return !(other < *this); }
  bool operator>=(const IPAddress& other) const { return !(*this < other); }
  explicit operator bool() const;

  Version version() const { return version_; }
  bool IsV4() const { return version_ == Version::kV4; }
  bool IsV6() const { return version_ == Version::kV6; }

  // These methods assume |x| is the appropriate size, but due to various
  // callers' casting needs we can't check them like the constructors above.
  // Callers should instead make any necessary checks themselves.
  void CopyToV4(uint8_t* x) const;
  void CopyToV6(uint8_t* x) const;

  // In some instances, we want direct access to the underlying byte storage,
  // in order to avoid making multiple copies.
  const uint8_t* bytes() const { return bytes_.data(); }

  // Parses a text representation of an IPv4 address (e.g. "192.168.0.1") or an
  // IPv6 address (e.g. "abcd::1234").
  static ErrorOr<IPAddress> Parse(const std::string& s);

 private:
  Version version_;
  std::array<uint8_t, 16> bytes_;
};

struct IPEndpoint {
 public:
  IPAddress address;
  uint16_t port = 0;

  explicit operator bool() const;

  // Parses a text representation of an IPv4/IPv6 address and port (e.g.
  // "192.168.0.1:8080" or "[abcd::1234]:8080").
  static ErrorOr<IPEndpoint> Parse(const std::string& s);

  std::string ToString() const;
};

bool operator==(const IPEndpoint& a, const IPEndpoint& b);
bool operator!=(const IPEndpoint& a, const IPEndpoint& b);

bool operator<(const IPEndpoint& a, const IPEndpoint& b);
inline bool operator>(const IPEndpoint& a, const IPEndpoint& b) {
  return b < a;
}
inline bool operator<=(const IPEndpoint& a, const IPEndpoint& b) {
  return !(b > a);
}
inline bool operator>=(const IPEndpoint& a, const IPEndpoint& b) {
  return !(a > b);
}

// Outputs a string of the form:
//      123.234.34.56
//   or fe80:0000:0000:0000:1234:5678:9abc:def0
std::ostream& operator<<(std::ostream& out, const IPAddress& address);

// Outputs a string of the form:
//      123.234.34.56:443
//   or [fe80:0000:0000:0000:1234:5678:9abc:def0]:8080
std::ostream& operator<<(std::ostream& out, const IPEndpoint& endpoint);

}  // namespace openscreen

#endif  // PLATFORM_BASE_IP_ADDRESS_H_
