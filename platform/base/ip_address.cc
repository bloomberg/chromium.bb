// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/ip_address.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

namespace openscreen {

// static
ErrorOr<IPAddress> IPAddress::Parse(const std::string& s) {
  ErrorOr<IPAddress> v4 = ParseV4(s);

  return v4 ? std::move(v4) : ParseV6(s);
}  // namespace openscreen

IPAddress::IPAddress() : version_(Version::kV4), bytes_({}) {}
IPAddress::IPAddress(const std::array<uint8_t, 4>& bytes)
    : version_(Version::kV4),
      bytes_{{bytes[0], bytes[1], bytes[2], bytes[3]}} {}
IPAddress::IPAddress(const uint8_t (&b)[4])
    : version_(Version::kV4), bytes_{{b[0], b[1], b[2], b[3]}} {}
IPAddress::IPAddress(Version version, const uint8_t* b) : version_(version) {
  if (version_ == Version::kV4) {
    bytes_ = {{b[0], b[1], b[2], b[3]}};
  } else {
    bytes_ = {{b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9],
               b[10], b[11], b[12], b[13], b[14], b[15]}};
  }
}
IPAddress::IPAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
    : version_(Version::kV4), bytes_{{b1, b2, b3, b4}} {}

IPAddress::IPAddress(const std::array<uint16_t, 8>& hextets)
    : IPAddress(hextets[0],
                hextets[1],
                hextets[2],
                hextets[3],
                hextets[4],
                hextets[5],
                hextets[6],
                hextets[7]) {}

IPAddress::IPAddress(const uint16_t (&hextets)[8])
    : IPAddress(hextets[0],
                hextets[1],
                hextets[2],
                hextets[3],
                hextets[4],
                hextets[5],
                hextets[6],
                hextets[7]) {}

IPAddress::IPAddress(uint16_t h0,
                     uint16_t h1,
                     uint16_t h2,
                     uint16_t h3,
                     uint16_t h4,
                     uint16_t h5,
                     uint16_t h6,
                     uint16_t h7)
    : version_(Version::kV6),
      bytes_{{
          static_cast<uint8_t>(h0 >> 8),
          static_cast<uint8_t>(h0),
          static_cast<uint8_t>(h1 >> 8),
          static_cast<uint8_t>(h1),
          static_cast<uint8_t>(h2 >> 8),
          static_cast<uint8_t>(h2),
          static_cast<uint8_t>(h3 >> 8),
          static_cast<uint8_t>(h3),
          static_cast<uint8_t>(h4 >> 8),
          static_cast<uint8_t>(h4),
          static_cast<uint8_t>(h5 >> 8),
          static_cast<uint8_t>(h5),
          static_cast<uint8_t>(h6 >> 8),
          static_cast<uint8_t>(h6),
          static_cast<uint8_t>(h7 >> 8),
          static_cast<uint8_t>(h7),
      }} {}

IPAddress::IPAddress(const IPAddress& o) noexcept = default;
IPAddress::IPAddress(IPAddress&& o) noexcept = default;
IPAddress& IPAddress::operator=(const IPAddress& o) noexcept = default;
IPAddress& IPAddress::operator=(IPAddress&& o) noexcept = default;

bool IPAddress::operator==(const IPAddress& o) const {
  if (version_ != o.version_)
    return false;

  if (version_ == Version::kV4) {
    return bytes_[0] == o.bytes_[0] && bytes_[1] == o.bytes_[1] &&
           bytes_[2] == o.bytes_[2] && bytes_[3] == o.bytes_[3];
  }
  return bytes_ == o.bytes_;
}

bool IPAddress::operator!=(const IPAddress& o) const {
  return !(*this == o);
}

IPAddress::operator bool() const {
  if (version_ == Version::kV4)
    return bytes_[0] | bytes_[1] | bytes_[2] | bytes_[3];

  for (const auto& byte : bytes_)
    if (byte)
      return true;

  return false;
}

void IPAddress::CopyToV4(uint8_t x[4]) const {
  assert(version_ == Version::kV4);
  std::memcpy(x, bytes_.data(), 4);
}

void IPAddress::CopyToV6(uint8_t x[16]) const {
  assert(version_ == Version::kV6);
  std::memcpy(x, bytes_.data(), 16);
}

// static
ErrorOr<IPAddress> IPAddress::ParseV4(const std::string& s) {
  int octets[4];
  int chars_scanned;
  if (sscanf(s.c_str(), "%3d.%3d.%3d.%3d%n", &octets[0], &octets[1], &octets[2],
             &octets[3], &chars_scanned) != 4 ||
      chars_scanned != static_cast<int>(s.size()) ||
      std::any_of(s.begin(), s.end(), [](char c) { return std::isspace(c); }) ||
      std::any_of(std::begin(octets), std::end(octets),
                  [](int octet) { return octet < 0 || octet > 255; })) {
    return Error::Code::kInvalidIPV4Address;
  }
  return IPAddress(octets[0], octets[1], octets[2], octets[3]);
}

namespace {

// Returns the zero-expansion of a double-colon in |s| if |s| is a
// well-formatted IPv6 address. If |s| is ill-formatted, returns *any* string
// that is ill-formatted.
std::string ExpandIPv6DoubleColon(const std::string& s) {
  constexpr char kDoubleColon[] = "::";
  const size_t double_colon_position = s.find(kDoubleColon);
  if (double_colon_position == std::string::npos) {
    return s;  // Nothing to expand.
  }
  if (double_colon_position != s.rfind(kDoubleColon)) {
    return {};  // More than one occurrence of double colons is illegal.
  }

  std::ostringstream expanded;
  const int num_single_colons = std::count(s.begin(), s.end(), ':') - 2;
  int num_zero_groups_to_insert = 8 - num_single_colons;
  if (double_colon_position != 0) {
    // abcd:0123:4567::f000:1
    // ^^^^^^^^^^^^^^^
    expanded << s.substr(0, double_colon_position + 1);
    --num_zero_groups_to_insert;
  }
  if (double_colon_position != (s.size() - 2)) {
    --num_zero_groups_to_insert;
  }
  while (--num_zero_groups_to_insert > 0) {
    expanded << "0:";
  }
  expanded << '0';
  if (double_colon_position != (s.size() - 2)) {
    // abcd:0123:4567::f000:1
    //                ^^^^^^^
    expanded << s.substr(double_colon_position + 1);
  }
  return expanded.str();
}

}  // namespace

// static
ErrorOr<IPAddress> IPAddress::ParseV6(const std::string& s) {
  const std::string scan_input = ExpandIPv6DoubleColon(s);
  uint16_t hextets[8];
  int chars_scanned;
  if (sscanf(scan_input.c_str(),
             "%4" SCNx16 ":%4" SCNx16 ":%4" SCNx16 ":%4" SCNx16 ":%4" SCNx16
             ":%4" SCNx16 ":%4" SCNx16 ":%4" SCNx16 "%n",
             &hextets[0], &hextets[1], &hextets[2], &hextets[3], &hextets[4],
             &hextets[5], &hextets[6], &hextets[7], &chars_scanned) != 8 ||
      chars_scanned != static_cast<int>(scan_input.size()) ||
      std::any_of(s.begin(), s.end(), [](char c) { return std::isspace(c); })) {
    return Error::Code::kInvalidIPV6Address;
  }
  return IPAddress(hextets);
}

IPEndpoint::operator bool() const {
  return address || port;
}

bool operator==(const IPEndpoint& a, const IPEndpoint& b) {
  return (a.address == b.address) && (a.port == b.port);
}

bool operator!=(const IPEndpoint& a, const IPEndpoint& b) {
  return !(a == b);
}

bool IPEndpointComparator::operator()(const IPEndpoint& a,
                                      const IPEndpoint& b) const {
  if (a.address.version() != b.address.version())
    return a.address.version() < b.address.version();
  if (a.address.IsV4()) {
    int ret = memcmp(a.address.bytes_.data(), b.address.bytes_.data(), 4);
    if (ret != 0)
      return ret < 0;
  } else {
    int ret = memcmp(a.address.bytes_.data(), b.address.bytes_.data(), 16);
    if (ret != 0)
      return ret < 0;
  }
  return a.port < b.port;
}

std::ostream& operator<<(std::ostream& out, const IPAddress& address) {
  uint8_t values[16];
  size_t len = 0;
  char separator;
  size_t values_per_separator;
  if (address.IsV4()) {
    out << std::dec;
    address.CopyToV4(values);
    len = 4;
    separator = '.';
    values_per_separator = 1;
  } else if (address.IsV6()) {
    out << std::hex;
    address.CopyToV6(values);
    len = 16;
    separator = ':';
    values_per_separator = 2;
  }
  out << std::setfill('0') << std::right;
  for (size_t i = 0; i < len; ++i) {
    if (i > 0 && (i % values_per_separator == 0)) {
      out << separator;
    }
    out << std::setw(2) << static_cast<int>(values[i]);
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const IPEndpoint& endpoint) {
  if (endpoint.address.IsV6()) {
    out << '[';
  }
  out << endpoint.address;
  if (endpoint.address.IsV6()) {
    out << ']';
  }
  return out << ':' << std::dec << static_cast<int>(endpoint.port);
}

}  // namespace openscreen
