// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTrialTokenValidator_h
#define WebTrialTokenValidator_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"

namespace blink {

enum class WebOriginTrialTokenStatus;

// This interface abstracts the task of validating a token for an experimental
// feature. Experimental features can be turned on and off at runtime for a
// specific renderer, depending on the presence of a valid token provided by
// the origin.
//
// For more information, see https://github.com/jpchase/OriginTrials.

class WebTrialTokenValidator {
 public:
  virtual ~WebTrialTokenValidator() {}

  // Returns whether the given token is valid for the specified origin. If the
  // token is valid, it also returns the feature the token is valid for in
  // |*feature_name|.
  virtual WebOriginTrialTokenStatus ValidateToken(const WebString& token,
                                                  const WebSecurityOrigin&,
                                                  WebString* feature_name) = 0;
};

}  // namespace blink

#endif  // WebTrialTokenValidator_h
