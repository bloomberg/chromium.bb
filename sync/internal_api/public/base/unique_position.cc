// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/unique_position.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "sync/protocol/unique_position.pb.h"

namespace syncer {

const size_t UniquePosition::kSuffixLength = 28;

// static.
bool UniquePosition::IsValidSuffix(const std::string& suffix) {
  // The suffix must be exactly the specified length, otherwise unique suffixes
  // are not sufficient to guarantee unique positions (because prefix + suffix
  // == p + refixsuffix).
  return suffix.length() == kSuffixLength;
}

// static.
bool UniquePosition::IsValidBytes(const std::string& bytes) {
  // The first condition ensures that our suffix uniqueness is sufficient to
  // guarantee position uniqueness.  Otherwise, it's possible the end of some
  // prefix + some short suffix == some long suffix.
  // The second condition ensures that FindSmallerWithSuffix can always return a
  // result.
  return bytes.length() >= kSuffixLength
      && bytes[bytes.length()-1] != 0;
}

// static.
UniquePosition UniquePosition::CreateInvalid() {
  UniquePosition pos;
  DCHECK(!pos.IsValid());
  return pos;
}

// static.
UniquePosition UniquePosition::FromProto(const sync_pb::UniquePosition& proto) {
  UniquePosition result(proto.value());
  return result;
}

// static.
UniquePosition UniquePosition::FromInt64(
    int64 x, const std::string& suffix) {
  uint64 y = static_cast<uint64>(x);
  y ^= 0x8000000000000000ULL; // Make it non-negative.
  std::string bytes(8, 0);
  for (int i = 7; i >= 0; --i) {
    bytes[i] = static_cast<uint8>(y);
    y >>= 8;
  }
  return UniquePosition(bytes, suffix);
}

// static.
UniquePosition UniquePosition::InitialPosition(
    const std::string& suffix) {
  DCHECK(IsValidSuffix(suffix));
  return UniquePosition("", suffix);
}

// static.
UniquePosition UniquePosition::Before(
    const UniquePosition& x,
    const std::string& suffix) {
  DCHECK(IsValidSuffix(suffix));
  DCHECK(x.IsValid());
  const std::string& before = FindSmallerWithSuffix(x.bytes_, suffix);
  return UniquePosition(before, suffix);
}

// static.
UniquePosition UniquePosition::After(
    const UniquePosition& x,
    const std::string& suffix) {
  DCHECK(IsValidSuffix(suffix));
  DCHECK(x.IsValid());
  const std::string& after = FindGreaterWithSuffix(x.bytes_, suffix);
  return UniquePosition(after, suffix);
}

// static.
UniquePosition UniquePosition::Between(
    const UniquePosition& before,
    const UniquePosition& after,
    const std::string& suffix) {
  DCHECK(before.IsValid());
  DCHECK(after.IsValid());
  DCHECK(before.LessThan(after));
  DCHECK(IsValidSuffix(suffix));
  const std::string& mid =
      FindBetweenWithSuffix(before.bytes_, after.bytes_, suffix);
  return UniquePosition(mid, suffix);
}

UniquePosition::UniquePosition() : is_valid_(false) {}

bool UniquePosition::LessThan(const UniquePosition& other) const {
  DCHECK(this->IsValid());
  DCHECK(other.IsValid());
  return bytes_ < other.bytes_;
}

bool UniquePosition::Equals(const UniquePosition& other) const {
  if (!this->IsValid() && !other.IsValid())
    return true;

  return bytes_ == other.bytes_;
}

void UniquePosition::ToProto(sync_pb::UniquePosition* proto) const {
  proto->set_value(bytes_);
}

void UniquePosition::SerializeToString(std::string* blob) const {
  DCHECK(blob);
  sync_pb::UniquePosition proto;
  ToProto(&proto);
  proto.SerializeToString(blob);
}

int64 UniquePosition::ToInt64() const {
  uint64 y = 0;
  const std::string& s = bytes_;
  size_t l = sizeof(int64);
  if (s.length() < l) {
    NOTREACHED();
    l = s.length();
  }
  for (size_t i = 0; i < l; ++i) {
    const uint8 byte = s[l - i - 1];
    y |= static_cast<uint64>(byte) << (i * 8);
  }
  y ^= 0x8000000000000000ULL;
  // This is technically implementation-defined if y > INT64_MAX, so
  // we're assuming that we're on a twos-complement machine.
  return static_cast<int64>(y);
}

bool UniquePosition::IsValid() const {
  return is_valid_;
}

std::string UniquePosition::ToDebugString() const {
  if (bytes_.empty())
    return std::string("INVALID[]");

  std::string debug_string = base::HexEncode(bytes_.data(), bytes_.length());
  if (!IsValid()) {
    debug_string = "INVALID[" + debug_string + "]";
  }
  return debug_string;;
}

std::string UniquePosition::GetSuffixForTest() const {
  const size_t prefix_len = bytes_.length() - kSuffixLength;
  return bytes_.substr(prefix_len, std::string::npos);
}

std::string UniquePosition::FindSmallerWithSuffix(
    const std::string& reference,
    const std::string& suffix) {
  size_t ref_zeroes = reference.find_first_not_of('\0');
  size_t suffix_zeroes = suffix.find_first_not_of('\0');

  // Neither of our inputs are allowed to have trailing zeroes, so the following
  // must be true.
  DCHECK_NE(ref_zeroes, std::string::npos);
  DCHECK_NE(suffix_zeroes, std::string::npos);

  if (suffix_zeroes > ref_zeroes) {
    // Implies suffix < ref.
    return "";
  }

  if (suffix.substr(suffix_zeroes) < reference.substr(ref_zeroes)) {
    // Prepend zeroes so the result has as many zero digits as |reference|.
    return std::string(ref_zeroes - suffix_zeroes, '\0');
  } else if (suffix_zeroes > 1) {
    // Prepend zeroes so the result has one more zero digit than |reference|.
    // We could also take the "else" branch below, but taking this branch will
    // give us a smaller result.
    return std::string(ref_zeroes - suffix_zeroes + 1, '\0');
  } else {
    // Prepend zeroes to match those in the |reference|, then something smaller
    // than the first non-zero digit in |reference|.
    char lt_digit = static_cast<uint8>(reference[ref_zeroes])/2;
    return std::string(ref_zeroes, '\0') + lt_digit;
  }
}

// static
std::string UniquePosition::FindGreaterWithSuffix(
    const std::string& reference,
    const std::string& suffix) {
  size_t ref_FFs = reference.find_first_not_of(kuint8max);
  size_t suffix_FFs = suffix.find_first_not_of(kuint8max);

  if (ref_FFs == std::string::npos) {
    ref_FFs = reference.length();
  }
  if (suffix_FFs == std::string::npos) {
    suffix_FFs = suffix.length();
  }

  if (suffix_FFs > ref_FFs) {
    // Implies suffix > reference.
    return "";
  }

  if (suffix.substr(suffix_FFs) > reference.substr(ref_FFs)) {
    // Prepend FF digits to match those in |reference|.
    return std::string(ref_FFs - suffix_FFs, kuint8max);
  } else if (suffix_FFs > 1) {
    // Prepend enough leading FF digits so result has one more of them than
    // |reference| does.  We could also take the "else" branch below, but this
    // gives us a smaller result.
    return std::string(ref_FFs - suffix_FFs + 1, kuint8max);
  } else {
    // Prepend FF digits to match those in |reference|, then something larger
    // than the first non-FF digit in |reference|.
    char gt_digit = static_cast<uint8>(reference[ref_FFs]) +
        (kuint8max - static_cast<uint8>(reference[ref_FFs]) + 1) / 2;
    return std::string(ref_FFs, kuint8max) + gt_digit;
  }
}

// TODO(rlarocque): Is there a better algorithm that we could use here?
// static
std::string UniquePosition::FindBetweenWithSuffix(
    const std::string& before,
    const std::string& after,
    const std::string& suffix) {
  DCHECK(IsValidSuffix(suffix));
  DCHECK_NE(before, after);
  DCHECK_LT(before, after);

  std::string mid;

  // Sometimes our suffix puts us where we want to be.
  if (before < suffix && suffix < after) {
    return "";
  }

  size_t i = 0;
  for ( ; i < std::min(before.length(), after.length()); ++i) {
    uint8 a_digit = before[i];
    uint8 b_digit = after[i];

    if (b_digit - a_digit >= 2) {
      mid.push_back(a_digit + (b_digit - a_digit)/2);
      return mid;
    } else if (a_digit == b_digit) {
      mid.push_back(a_digit);

      // Both strings are equal so far.  Will appending the suffix at this point
      // give us the comparison we're looking for?
      if (before.substr(i+1) < suffix && suffix < after.substr(i+1)) {
        return mid;
      }
    } else {
      DCHECK_EQ(b_digit - a_digit, 1);  // Implied by above if branches.

      // The two options are off by one digit.  The choice of whether to round
      // up or down here will have consequences on what we do with the remaining
      // digits.  Exploring both options is an optimization and is not required
      // for the correctness of this algorithm.

      // Option A: Round down the current digit.  This makes our |mid| <
      // |after|, no matter what we append afterwards.  We then focus on
      // appending digits until |mid| > |before|.
      std::string mid_a = mid;
      mid_a.push_back(a_digit);
      mid_a.append(FindGreaterWithSuffix(before.substr(i+1), suffix));

      // Option B: Round up the current digit.  This makes our |mid| > |before|,
      // no matter what we append afterwards.  We then focus on appending digits
      // until |mid| < |after|.  Note that this option may not be viable if the
      // current digit is the last one in |after|, so we skip the option in that
      // case.
      if (after.length() > i+1) {
        std::string mid_b = mid;
        mid_b.push_back(b_digit);
        mid_b.append(FindSmallerWithSuffix(after.substr(i+1), suffix));

        // Does this give us a shorter position value?  If so, use it.
        if (mid_b.length() < mid_a.length()) {
          return mid_b;
        }
      }
      return mid_a;
    }
  }

  // If we haven't found a midpoint yet, the following must be true:
  DCHECK_EQ(before.substr(0, i), after.substr(0, i));
  DCHECK_EQ(before, mid);
  DCHECK_LT(before.length(), after.length());

  // We know that we'll need to append at least one more byte to |mid| in the
  // process of making it < |after|.  Appending any digit, regardless of the
  // value, will make |before| < |mid|.  Therefore, the following will get us a
  // valid position.

  mid.append(FindSmallerWithSuffix(after.substr(i), suffix));
  return mid;
}

UniquePosition::UniquePosition(const std::string& internal_rep)
    : bytes_(internal_rep),
      is_valid_(IsValidBytes(bytes_)) {
}

UniquePosition::UniquePosition(
    const std::string& prefix,
    const std::string& suffix)
  : bytes_(prefix + suffix),
    is_valid_(IsValidBytes(bytes_)) {
  DCHECK(IsValidSuffix(suffix));
  DCHECK(IsValid());
}

}  // namespace syncer
