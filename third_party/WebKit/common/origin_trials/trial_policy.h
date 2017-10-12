// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_ORIGIN_TRIALS_TRIAL_POLICY_H_
#define THIRD_PARTY_WEBKIT_COMMON_ORIGIN_TRIALS_TRIAL_POLICY_H_

#include "base/strings/string_piece.h"
#include "third_party/WebKit/common/common_export.h"
#include "url/gurl.h"

namespace blink {

class BLINK_COMMON_EXPORT TrialPolicy {
 public:
  virtual ~TrialPolicy() = default;

  virtual bool IsOriginTrialsSupported() const = 0;

  virtual base::StringPiece GetPublicKey() const = 0;
  virtual bool IsFeatureDisabled(base::StringPiece feature) const = 0;
  virtual bool IsTokenDisabled(base::StringPiece token_signature) const = 0;
  virtual bool IsOriginSecure(const GURL& url) const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_COMMON_ORIGIN_TRIALS_TRIAL_POLICY_H_
