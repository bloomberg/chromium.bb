// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTrialTokenValidator_h
#define WebTrialTokenValidator_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebString.h"

namespace blink {

// This interface abstracts the task of validating a token for an experimental
// feature. Experimental features can be turned on and off at runtime for a
// specific renderer, depending on the presence of a valid token provided by
// the origin.
//
// More documentation on the design of the experimental framework is at
// https://docs.google.com/document/d/1qVP2CK1lbfmtIJRIm6nwuEFFhGhYbtThLQPo3CSTtmg

class WebTrialTokenValidator {
public:
    virtual ~WebTrialTokenValidator() {}

    // Returns true if the given token is valid for the specified origin and
    // feature name.
    virtual bool validateToken(const WebString& token, const WebString& origin, const WebString& featureName) = 0;
};

} // namespace blink

#endif // WebTrialTokenValidator_h
