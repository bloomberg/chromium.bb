/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

//
// checked_cast.h
//
// A template system, intended to be a drop-in replacement for static_cast<>,
// which performs compile-time and (if necessary) runtime evaluation of
// integral type casts to detect and handle arithmetic overflow errors.
//
#ifndef NATIVE_CLIENT_SRC_INCLUDE_CHECKED_CAST_H_
#define NATIVE_CLIENT_SRC_INCLUDE_CHECKED_CAST_H_ 1

// Windows defines std::min and std::max in a different header
// than gcc.
#if NACL_WINDOWS
#include <xutility>
#endif

#include <limits>

#if !NACL_WINDOWS
// this is where std::min/max SHOULD live. It's included here (rather than
// in an else block along with the Windows include above) to avoid breaking
// google cpplint's header file inclusion order rules.
#include <algorithm>
#endif

// TODO(ilewis): remove reference to base as soon as we can get COMPILE_ASSERT
//                from another source.
#include "base/basictypes.h"
#include "native_client/src/shared/platform/nacl_log.h"

namespace nacl {

  template <
    typename target_t,
    typename source_t,
    template<typename, typename> class trunc_policy>
  INLINE target_t checked_cast(const source_t& input);

  //
  // Convenience wrappers for specializations of checked_cast with
  // different truncation policies
  //

  template<typename T, typename S>
  INLINE T checked_cast_fatal(const S& input);

  template<typename T, typename S>
  INLINE T saturate_cast(const S& input);

  //
  // Helper function prototypes
  //
  //
  namespace CheckedCastDetail {

    template <typename target_t, typename source_t>
    INLINE bool IsTrivialCast();

    template <typename target_t, typename source_t>
    INLINE bool ValidCastRange(source_t* rangeMin,
                               source_t* rangeMax);

    //
    // OnTruncate* functions: policy functions that define
    // what to do if a checked cast can't be made without
    // truncation that loses data.
    //
    template <typename target_t, typename source_t>
    struct TruncationPolicySaturate {
      static target_t OnTruncate(const source_t& input);
    };

    template <typename target_t, typename source_t>
    struct TruncationPolicyAbort {
      static target_t OnTruncate(const source_t& input);
    };

  }  // namespace CheckedCastDetail

}  // namespace nacl


//-----------------------------------------------------------------------------
//
// Implementation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// bool IsTrivialCast()
//
// Determine whether a cast between two numeric types can
// be done trivially, i.e. with no loss of precision when
// casting a value of type src_t to a value of type target_t.
//
// Requires:
//    A specialization of std::numeric_limits<> must exist for both types.
//
// Returns:
//    true:   types can be converted trivially.
//    false:  types cannot be converted trivially, or unknown.
//
//-----------------------------------------------------------------------------
template <typename target_t, typename source_t>
INLINE bool nacl::CheckedCastDetail::IsTrivialCast() {
  typedef std::numeric_limits<target_t> target_limits;
  typedef std::numeric_limits<source_t> source_limits;
  // This code relies somewhat on the optimizer to be
  // smart--it should be able to run these checks at
  // compile time, not runtime. The alternative is to
  // do the same thing with template metaprogramming.
  // I don't like that alternative, so let's trust
  // the optimizer unless and until it's proven
  // untrustworthy.
  bool result = true;

  // Precondition check: types must have specializations in
  // std::numeric_limits--otherwise there's not enough info
  // to make this decision
  result = result && target_limits::is_specialized;
  result = result && source_limits::is_specialized;

  // Initial trivial check: assume that target size must
  // be greater or equal to source size if we're going to
  // hold all possible source values.
  result = result && (sizeof(target_t) >= sizeof(source_t));

  // Check for float/int mismatch
  result = result && (target_limits::is_integer
    == source_limits::is_integer);

  // Make sure the signedness of the two types is identical
  result = result &&
          (target_limits::is_signed == source_limits::is_signed);

  // Range check--this will catch issues with UDTs that define
  // weird values for min and max.
  if (result) {
    // we checked for signedness mismatches earlier, so it's
    // safe to disable signed/unsigned comparison warnings.
    // TODO(ilewis): port to gcc
#pragma warning(push)
#pragma warning(disable:4018)
    result = result && (target_limits::max() >= source_limits::max());
    result = result && (target_limits::min() <= source_limits::min());
#pragma warning(pop)
  }

  return result;
}

//-----------------------------------------------------------------------------
// bool ValidCastRange(source_& out_rangeMin, source_t& out_rangeMax)
//
// Finds the minimum and maximum values of source_t that can be represented
// using type target_t.
// For instance:
//    ValidCastRange<int8,uint64>() => 0, 127
// In this example, the range's upper bound is determined by the maximum value
// of an int8. But the lower bound is not -128, which is int8's minimum value.
// Although -128 is a valid value for int8, there is no way to represent the
// value -128 using a uint64. Therefore the smallest value of uint64 that can
// be safely cast to an int8 is 0, and the largest such value is 127.
//
// In the event that the two types have no overlap, or the function lacks the
// information to determine whether the types overlap, the return value is
// false.
//
// Parameters:
//    out_rangeMin: out parameter which will be filled with the lower bound
//                  of the valid range if the function is successful.
//    out_rangeMax: out parameter which will be filled with the upper bound
//                  of the valid range if the function is successful.
//
// Returns:
//    true:   if the function could determine the valid range for a cast from
//            source_t to target_t
//    false:  if no such range exists or if the range cannot be determined.
//-----------------------------------------------------------------------------
template <typename target_t, typename source_t>
INLINE bool nacl::CheckedCastDetail::ValidCastRange(
    source_t* rangeMin,
    source_t* rangeMax) {
  typedef std::numeric_limits<target_t> target_limits;
  typedef std::numeric_limits<source_t> source_limits;

  // save off these values so we don't have to keep calling numeric_limits.
  source_t sourceMin = source_limits::min();
  source_t sourceMax = source_limits::max();
  target_t targetMin = target_limits::min();
  target_t targetMax = target_limits::max();

  bool valid = true;

  //
  // There are two major cases we need to handle:
  //  1. The types are either both signed, or both unsigned
  //  2. One type is signed while the other is unsigned.
  //
  // It turns out that we don't even need to know which type
  // (source or target) is signed and which is unsigned; it's
  // enough just knowing whether the signedness matches or
  // doesn't match.
  //
#pragma warning(push)          // disable signed/unsigned comparison warnings,
#pragma warning(disable:4018)  // since we're checking explicitly for
                               // mismatched signedness.

  if (source_limits::is_signed != target_limits::is_signed) {
    // If we get here, then the signedness differs. This means that the
    // ranges may be overlapping or disjoint, and the range of the smaller type
    // is no longer guaranteed to be a proper subset of the larger type's
    // range.
    //
    if (sourceMax < 0 || targetMax < 0) {
      // if either maximum is negative, there's no chance the cast is valid.
      valid = false;
    } else {
      // clamp negative minima to zero.
      sourceMin = std::max(sourceMin, static_cast<source_t>(0));
      targetMin = std::max(targetMin, static_cast<target_t>(0));
    }
  }
  // the casts here are safe because we've already determined that
  // (1) source_t is wider than target_t and (2) if the signedness differs,
  // the minima have been clamped to zero
  *rangeMin = std::max(sourceMin, static_cast<source_t>(targetMin));
  *rangeMax = std::min(sourceMax, static_cast<source_t>(targetMax));

  // Weird corner case: it's conceivable that two UDTs could define
  // ranges that don't actually overlap. In that case there is no valid
  // range of values for the cast. We can decide if this has happened by
  // comparing rangeMin against rangeMax--if rangeMin is greater than
  // rangeMax then we know something's screwy.
  if (*rangeMin > *rangeMax) {
    valid = false;
  }
#pragma warning(pop)  // re-enable signed/unsigned comparison warnings

  return valid;
}

//-----------------------------------------------------------------------------
// checked_cast(const source_t& input, trunc_fn& OnTrunc)
//    An augmented replacement for static_cast. Does range checking,
//    overflow validation. Includes a policy which allows the caller to
//    specify what action to take if it's determined that the cast
//    would lose data.
//
// Template parameters:
//    target_t:     the type on the left hand side of the cast
//    source_t:     the type on the right hand side of the cast
//    trunc_policy: type of a policy object that will be called
//                if the cast would result in a loss of data. The
//                type must implement a method named OnTruncate which
//                is compatible with the following signature:
//                target_t (target_t& output, const source_t& input).
//                It is the responsibility of this function to
//                convert the value of 'input' and store the result
//                of the conversion in out parameter 'output'. It is
//                permissible for the function to abort or log warnings
//                if that is the desired behavior.
//
// Function parameters:
//    input:    the value to convert.
//    onTrunc:  reference to the function or functor which will be
//              called if conversion would lose data. See notes regarding
//              template parameter trunc_fn for more details.
//
// usage:
//    // naive truncation handler for sample purposes ONLY!!
//    template <typename T, typename S>
//    void naive_trunc(T& o, const S& i) {
//      // this only gets called if checked_cast can't do a safe automatic
//      // conversion. In real code you'd want to do something cool like
//      // clamp the incoming value or abort the program. For this sample
//      // we just return a c-style cast, which makes the outcome roughly
//      // equivalent to static_cast<>.
//      o = (T)i;
//    }
//
//    void main() {
//      uint64_t foo = 0xffff0000ffff0000;
//      uint32_t bar = checked_cast<uint32_t>(foo, naive_trunc);
//    }
//
template <
    typename target_t,
    typename source_t,
    template<typename, typename> class trunc_policy>
INLINE target_t nacl::checked_cast(const source_t& input) {
  typedef std::numeric_limits<target_t> target_limits;
  typedef std::numeric_limits<source_t> source_limits;

  target_t output;

  //
  // Sanity checks: early failure for types we can't recognize
  // TODO(ilewis): replace COMPILE_ASSERT with code that doesn't
  // come from Chromium(?)
  //

  // need a specialization of std::numeric_limits in order to
  // make informed decisions about the type
  COMPILE_ASSERT((source_limits::is_specialized == true),
                  checked_cast_unsupported_type_on_right_hand_side);
  COMPILE_ASSERT((target_limits::is_specialized == true),
                  checked_cast_unsupported_type_on_left_hand_side);

  // Only integers are supported; non-integral types have very
  // different issues.
  COMPILE_ASSERT((source_limits::is_integer == true),
                  checked_cast_nonintegral_type_on_right_hand_side);
  COMPILE_ASSERT((target_limits::is_integer == true),
                  checked_cast_nonintegral_type_on_left_hand_side);

  //
  // Check to see if the cast is trivial enough to leave entirely
  // to the compiler
  //
  if (CheckedCastDetail::IsTrivialCast<target_t, source_t>()) {
    // always works
    return static_cast<target_t>(input);
  }

  //
  // There is no trivial conversion, so we need to check the actual
  // value of the input against the range that the target type supports.
  //
  source_t rangeMin, rangeMax;

  bool valid = CheckedCastDetail::ValidCastRange<target_t, source_t>(
                                      &rangeMin, &rangeMax);

  if (valid
      && input <= rangeMax
      && input >= rangeMin) {
    // safe to do this conversion
    output = static_cast<target_t>(input);
  } else {
    // unsafe to do a conversion; hand off to the truncation policy
    output = trunc_policy<target_t, source_t>::OnTruncate(input);
  }

  // Done!
  return output;
}

//
// Convenience wrappers for specializations of checked_cast
//

//-----------------------------------------------------------------------------
// checked_cast_fatal(const S& input)
// Calls checked_cast; on truncation, log error and abort
//-----------------------------------------------------------------------------
template<typename T, typename S>
INLINE T nacl::checked_cast_fatal(const S& input) {
  return checked_cast<T, S, CheckedCastDetail::TruncationPolicyAbort>(input);
}
//-----------------------------------------------------------------------------
// saturate_cast(const S& input)
// Calls checked_cast; on truncation, saturates the input to the minimum
// or maximum of the output.
//-----------------------------------------------------------------------------
template<typename T, typename S>
INLINE T nacl::saturate_cast(const S& input) {
  return
    checked_cast<T, S, CheckedCastDetail::TruncationPolicySaturate>(input);
}

//-----------------------------------------------------------------------------
// CheckedCastDetail::OnTruncationSaturate
// Implements the Saturate truncation policy.
//-----------------------------------------------------------------------------
template <typename target_t, typename source_t>
INLINE target_t nacl
    ::CheckedCastDetail
    ::TruncationPolicySaturate<target_t, source_t>
    ::OnTruncate(const source_t& input) {

  source_t clamped = input;
  source_t min, max;
  bool valid = ValidCastRange<target_t, source_t>(&min, &max);

  if (!valid) {
    NaClLog(LOG_FATAL, "Checked cast: type ranges do not overlap");
  }

  clamped = std::min(clamped, max);
  clamped = std::max(clamped, min);
  target_t output = static_cast<target_t>(clamped);

  return output;
}



//-----------------------------------------------------------------------------
// CheckedCastDetail::OnTruncationAbort
// Implements the Abort truncation policy.
//-----------------------------------------------------------------------------
template <typename target_t, typename source_t>
INLINE target_t nacl
    ::CheckedCastDetail
    ::TruncationPolicyAbort<target_t, source_t>
    ::OnTruncate(const source_t&) {
  NaClLog(LOG_FATAL, "Arithmetic overflow");

  // Unreachable, assuming that LOG_FATAL really is fatal
  return 0;
}

#endif  /* NATIVE_CLIENT_SRC_INCLUDE_CHECKED_CAST_H_ */

