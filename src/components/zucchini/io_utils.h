// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_IO_UTILS_H_
#define COMPONENTS_ZUCCHINI_IO_UTILS_H_

#include <stdint.h>

#include <cctype>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

#include "base/macros.h"

namespace zucchini {

// An std::ostream wrapper that that limits number of std::endl lines to output,
// useful for preventing excessive debug message output. Usage requires some
// work by the caller. Sample:
//   static LimitedOutputStream los(std::cerr, 10);
//   if (!los.full()) {
//     ...  // Prepare message. Block may be skipped so don't do other work!
//     los << message;
//     los << std::endl;  // Important!
//   }
class LimitedOutputStream : public std::ostream {
 private:
  class StreamBuf : public std::stringbuf {
   public:
    StreamBuf(std::ostream& os, int limit);
    ~StreamBuf() override;

    int sync() override;
    bool full() const { return counter_ >= limit_; }

   private:
    std::ostream& os_;
    const int limit_;
    int counter_ = 0;
  };

 public:
  LimitedOutputStream(std::ostream& os, int limit);
  bool full() const { return buf_.full(); }

 private:
  StreamBuf buf_;

  DISALLOW_COPY_AND_ASSIGN(LimitedOutputStream);
};

// A class to render hexadecimal numbers for std::ostream with 0-padding. This
// is more concise and flexible than stateful STL manipulator alternatives; so:
//   std::ios old_fmt(nullptr);
//   old_fmt.copyfmt(std::cout);
//   std::cout << std::uppercase << std::hex;
//   std::cout << std::setfill('0') << std::setw(8) << int_data << std::endl;
//   std::cout.copyfmt(old_fmt);
// can be expressed as:
//   std::cout << AxHex<8>(int_data) << std::endl;
template <int N, typename T = uint32_t>
struct AsHex {
  explicit AsHex(T value_in) : value(value_in) {}
  T value;
};

template <int N, typename T>
std::ostream& operator<<(std::ostream& os, const AsHex<N, T>& as_hex) {
  char buf[N + 1];
  buf[N] = '\0';
  T value = as_hex.value;
  for (int i = N - 1; i >= 0; --i, value >>= 4)
    buf[i] = "0123456789ABCDEF"[static_cast<int>(value & 0x0F)];
  if (value)
    os << "...";  // To indicate data truncation, or negative values.
  os << buf;
  return os;
}

// An output manipulator to simplify printing list separators. Sample usage:
//   PrefixSep sep(",");
//   for (int i : {3, 1, 4, 1, 5, 9})
//      std::cout << sep << i;
//   std::cout << std::endl;  // Outputs "3,1,4,1,5,9\n".
class PrefixSep {
 public:
  explicit PrefixSep(const std::string& sep_str) : sep_str_(sep_str) {}

  friend std::ostream& operator<<(std::ostream& ostr, PrefixSep& obj);

 private:
  std::string sep_str_;
  bool first_ = true;

  DISALLOW_COPY_AND_ASSIGN(PrefixSep);
};

// An input manipulator that dictates the expected next character in
// |std::istream|, and invalidates the stream if expectation is not met.
class EatChar {
 public:
  explicit EatChar(char ch) : ch_(ch) {}

  friend inline std::istream& operator>>(std::istream& istr,
                                         const EatChar& obj) {
    if (!istr.fail() && istr.get() != obj.ch_)
      istr.setstate(std::ios_base::failbit);
    return istr;
  }

 private:
  char ch_;

  DISALLOW_COPY_AND_ASSIGN(EatChar);
};

// An input manipulator that reads an unsigned integer from |std::istream|,
// and invalidates the stream on failure. Intolerant of leading white spaces,
template <typename T>
class StrictUInt {
 public:
  explicit StrictUInt(T& var) : var_(var) {}
  StrictUInt(const StrictUInt&) = default;

  friend std::istream& operator>>(std::istream& istr, StrictUInt<T> obj) {
    if (!istr.fail() && !::isdigit(istr.peek())) {
      istr.setstate(std::ios_base::failbit);
      return istr;
    }
    return istr >> obj.var_;
  }

 private:
  T& var_;
};

// Stub out uint8_t: istream treats it as char, and value won't be read as int!
template <>
struct StrictUInt<uint8_t> {};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_IO_UTILS_H_
