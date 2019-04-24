/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <numeric>

#include "modules/audio_coding/neteq/histogram.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

Histogram::Histogram(size_t num_buckets, int forget_factor)
    : buckets_(num_buckets, 0),
      forget_factor_(0),
      base_forget_factor_(forget_factor) {}

Histogram::~Histogram() {}

// Each element in the vector is first multiplied by the forgetting factor
// |forget_factor_|. Then the vector element indicated by |iat_packets| is then
// increased (additive) by 1 - |forget_factor_|. This way, the probability of
// |iat_packets| is slightly increased, while the sum of the histogram remains
// constant (=1).
// Due to inaccuracies in the fixed-point arithmetic, the histogram may no
// longer sum up to 1 (in Q30) after the update. To correct this, a correction
// term is added or subtracted from the first element (or elements) of the
// vector.
// The forgetting factor |forget_factor_| is also updated. When the DelayManager
// is reset, the factor is set to 0 to facilitate rapid convergence in the
// beginning. With each update of the histogram, the factor is increased towards
// the steady-state value |kIatFactor_|.
void Histogram::Add(int value) {
  RTC_DCHECK(value >= 0);
  RTC_DCHECK(value < static_cast<int>(buckets_.size()));
  int vector_sum = 0;  // Sum up the vector elements as they are processed.
  // Multiply each element in |buckets_| with |forget_factor_|.
  for (int& bucket : buckets_) {
    bucket = (static_cast<int64_t>(bucket) * forget_factor_) >> 15;
    vector_sum += bucket;
  }

  // Increase the probability for the currently observed inter-arrival time
  // by 1 - |forget_factor_|. The factor is in Q15, |buckets_| in Q30.
  // Thus, left-shift 15 steps to obtain result in Q30.
  buckets_[value] += (32768 - forget_factor_) << 15;
  vector_sum += (32768 - forget_factor_) << 15;  // Add to vector sum.

  // |buckets_| should sum up to 1 (in Q30), but it may not due to
  // fixed-point rounding errors.
  vector_sum -= 1 << 30;  // Should be zero. Compensate if not.
  if (vector_sum != 0) {
    // Modify a few values early in |buckets_|.
    int flip_sign = vector_sum > 0 ? -1 : 1;
    for (int& bucket : buckets_) {
      // Add/subtract 1/16 of the element, but not more than |vector_sum|.
      int correction = flip_sign * std::min(abs(vector_sum), bucket >> 4);
      bucket += correction;
      vector_sum += correction;
      if (abs(vector_sum) == 0) {
        break;
      }
    }
  }
  RTC_DCHECK(vector_sum == 0);  // Verify that the above is correct.

  // Update |forget_factor_| (changes only during the first seconds after a
  // reset). The factor converges to |base_forget_factor_|.
  forget_factor_ += (base_forget_factor_ - forget_factor_ + 3) >> 2;
}

int Histogram::Quantile(int probability) {
  // Find the bucket for which the probability of observing an
  // inter-arrival time larger than or equal to |index| is larger than or
  // equal to |probability|. The sought probability is estimated using
  // the histogram as the reverse cumulant PDF, i.e., the sum of elements from
  // the end up until |index|. Now, since the sum of all elements is 1
  // (in Q30) by definition, and since the solution is often a low value for
  // |iat_index|, it is more efficient to start with |sum| = 1 and subtract
  // elements from the start of the histogram.
  int inverse_probability = (1 << 30) - probability;
  size_t index = 0;        // Start from the beginning of |buckets_|.
  int sum = 1 << 30;       // Assign to 1 in Q30.
  sum -= buckets_[index];  // Ensure that target level is >= 1.

  do {
    // Subtract the probabilities one by one until the sum is no longer greater
    // than |inverse_probability|.
    ++index;
    sum -= buckets_[index];
  } while ((sum > inverse_probability) && (index < buckets_.size() - 1));
  return static_cast<int>(index);
}

// Set the histogram vector to an exponentially decaying distribution
// buckets_[i] = 0.5^(i+1), i = 0, 1, 2, ...
// buckets_ is in Q30.
void Histogram::Reset() {
  // Set temp_prob to (slightly more than) 1 in Q14. This ensures that the sum
  // of buckets_ is 1.
  uint16_t temp_prob = 0x4002;  // 16384 + 2 = 100000000000010 binary.
  for (int& bucket : buckets_) {
    temp_prob >>= 1;
    bucket = temp_prob << 16;
  }
  forget_factor_ = 0;  // Adapt the histogram faster for the first few packets.
}

int Histogram::NumBuckets() const {
  return buckets_.size();
}

void Histogram::Scale(int old_bucket_width, int new_bucket_width) {
  buckets_ = ScaleBuckets(buckets_, old_bucket_width, new_bucket_width);
}

std::vector<int> Histogram::ScaleBuckets(const std::vector<int>& buckets,
                                         int old_bucket_width,
                                         int new_bucket_width) {
  RTC_DCHECK_GT(old_bucket_width, 0);
  RTC_DCHECK_GT(new_bucket_width, 0);
  RTC_DCHECK_EQ(old_bucket_width % 10, 0);
  RTC_DCHECK_EQ(new_bucket_width % 10, 0);
  std::vector<int> new_histogram(buckets.size(), 0);
  int64_t acc = 0;
  int time_counter = 0;
  size_t new_histogram_idx = 0;
  for (size_t i = 0; i < buckets.size(); i++) {
    acc += buckets[i];
    time_counter += old_bucket_width;
    // The bins should be scaled, to ensure the histogram still sums to one.
    const int64_t scaled_acc = acc * new_bucket_width / time_counter;
    int64_t actually_used_acc = 0;
    while (time_counter >= new_bucket_width) {
      const int64_t old_histogram_val = new_histogram[new_histogram_idx];
      new_histogram[new_histogram_idx] =
          rtc::saturated_cast<int>(old_histogram_val + scaled_acc);
      actually_used_acc += new_histogram[new_histogram_idx] - old_histogram_val;
      new_histogram_idx =
          std::min(new_histogram_idx + 1, new_histogram.size() - 1);
      time_counter -= new_bucket_width;
    }
    // Only subtract the part that was succesfully written to the new histogram.
    acc -= actually_used_acc;
  }
  // If there is anything left in acc (due to rounding errors), add it to the
  // last bin. If we cannot add everything to the last bin we need to add as
  // much as possible to the bins after the last bin (this is only possible
  // when compressing a histogram).
  while (acc > 0 && new_histogram_idx < new_histogram.size()) {
    const int64_t old_histogram_val = new_histogram[new_histogram_idx];
    new_histogram[new_histogram_idx] =
        rtc::saturated_cast<int>(old_histogram_val + acc);
    acc -= new_histogram[new_histogram_idx] - old_histogram_val;
    new_histogram_idx++;
  }
  RTC_DCHECK_EQ(buckets.size(), new_histogram.size());
  if (acc == 0) {
    // If acc is non-zero, we were not able to add everything to the new
    // histogram, so this check will not hold.
    RTC_DCHECK_EQ(accumulate(buckets.begin(), buckets.end(), 0ll),
                  accumulate(new_histogram.begin(), new_histogram.end(), 0ll));
  }
  return new_histogram;
}

}  // namespace webrtc
