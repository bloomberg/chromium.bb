// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ISOLATION_KEY_H_
#define NET_BASE_NETWORK_ISOLATION_KEY_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "url/origin.h"

namespace net {

// Key used to isolate shared network stack resources used by requests based on
// the context on which they were made.
class NET_EXPORT NetworkIsolationKey {
 public:
  // Full constructor.  When a request is initiated by the top frame, it must
  // also populate the |frame_origin| parameter when calling this constructor.
  explicit NetworkIsolationKey(const url::Origin& top_frame_origin,
                               const url::Origin& frame_origin);

  // Construct an empty key.
  NetworkIsolationKey();

  NetworkIsolationKey(const NetworkIsolationKey& network_isolation_key);

  ~NetworkIsolationKey();

  NetworkIsolationKey& operator=(
      const NetworkIsolationKey& network_isolation_key);
  NetworkIsolationKey& operator=(NetworkIsolationKey&& network_isolation_key);

  // Creates a transient non-empty NetworkIsolationKey by creating an opaque
  // origin. This prevents the NetworkIsolationKey from sharing data with other
  // NetworkIsolationKeys. Data for transient NetworkIsolationKeys is not
  // persisted to disk.
  static NetworkIsolationKey CreateTransient();

  // Creates a new key using |top_frame_origin_| and |new_frame_origin|.
  NetworkIsolationKey CreateWithNewFrameOrigin(
      const url::Origin& new_frame_origin) const;

  // Intended for temporary use in locations that should be using a non-empty
  // NetworkIsolationKey(), but are not yet. This both reduces the chance of
  // accidentally copying the lack of a NIK where one should be used, and
  // provides a reasonable way of locating callsites that need to have their
  // NetworkIsolationKey filled in.
  static NetworkIsolationKey Todo() { return NetworkIsolationKey(); }

  // Compare keys for equality, true if all enabled fields are equal.
  bool operator==(const NetworkIsolationKey& other) const {
    return top_frame_origin_ == other.top_frame_origin_ &&
           frame_origin_ == other.frame_origin_;
  }

  // Compare keys for inequality, true if any enabled field varies.
  bool operator!=(const NetworkIsolationKey& other) const {
    return (top_frame_origin_ != other.top_frame_origin_) ||
           (frame_origin_ != other.frame_origin_);
  }

  // Provide an ordering for keys based on all enabled fields.
  bool operator<(const NetworkIsolationKey& other) const {
    return top_frame_origin_ < other.top_frame_origin_ ||
           (top_frame_origin_ == other.top_frame_origin_ &&
            frame_origin_ < other.frame_origin_);
  }

  // Returns the string representation of the key, which is the string
  // representation of each piece of the key separated by spaces.
  std::string ToString() const;

  // Returns string for debugging. Difference from ToString() is that transient
  // entries may be distinguishable from each other.
  std::string ToDebugString() const;

  // Returns true if all parts of the key are non-empty.
  bool IsFullyPopulated() const;

  // Returns true if this key's lifetime is short-lived, or if
  // IsFullyPopulated() returns true. It may not make sense to persist state to
  // disk related to it (e.g., disk cache).
  bool IsTransient() const;

  // Getters for the original top frame and frame origins used as inputs to
  // construct |this|. This could return different values from what the
  // isolation key eventually uses based on whether the NIK uses eTLD+1 or not.
  // WARNING(crbug.com/1032081): Note that these might not return the correct
  // value associated with a request if the NIK on which this is called is from
  // a component using multiple requests mapped to the same NIK.
  const base::Optional<url::Origin>& GetTopFrameOrigin() const {
    DCHECK_EQ(original_top_frame_origin_.has_value(),
              top_frame_origin_.has_value());
    return original_top_frame_origin_;
  }
  const base::Optional<url::Origin>& GetFrameOrigin() const {
    DCHECK_EQ(original_frame_origin_.has_value(), frame_origin_.has_value());
    return original_frame_origin_;
  }

  // Returns true if all parts of the key are empty.
  bool IsEmpty() const;

  // Returns a representation of |this| as a base::Value. Returns false on
  // failure. Succeeds if either IsEmpty() or !IsTransient().
  bool ToValue(base::Value* out_value) const WARN_UNUSED_RESULT;

  // Inverse of ToValue(). Writes the result to |network_isolation_key|. Returns
  // false on failure. Fails on values that could not have been produced by
  // ToValue(), like transient origins. If the value of
  // net::features::kAppendFrameOriginToNetworkIsolationKey has changed between
  // saving and loading the data, fails.
  static bool FromValue(const base::Value& value,
                        NetworkIsolationKey* out_network_isolation_key)
      WARN_UNUSED_RESULT;

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkIsolationKeyWithFrameOriginTest,
                           UseRegistrableDomain);

  void ReplaceOriginsWithRegistrableDomains();

  // Whether or not to use the |frame_origin_| as part of the key.
  bool use_frame_origin_;

  // The origin/etld+1 of the top frame of the page making the request.
  base::Optional<url::Origin> top_frame_origin_;

  // The original top frame origin sent to the constructor of this request.
  base::Optional<url::Origin> original_top_frame_origin_;

  // The origin/etld+1 of the frame that initiates the request.
  base::Optional<url::Origin> frame_origin_;

  // The original frame origin sent to the constructor of this request.
  base::Optional<url::Origin> original_frame_origin_;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_ISOLATION_KEY_H_
