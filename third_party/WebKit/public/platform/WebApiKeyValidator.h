// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebApiKeyValidator_h
#define WebApiKeyValidator_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebString.h"

namespace blink {

// This interface abstracts the task of validating a key for an experimental
// API. Experimental APIs can be turned on and off at runtime for a specific
// renderer, depending on the presence of a valid key in the document's head.
// The details of determining whether a key is valid for a particular API is
// left up to the embedder. An implementation can effectively disable all API
// experiments by simply returning false in all cases.
//
// More documentation on the design of the experimental framework is at
// https://docs.google.com/document/d/1qVP2CK1lbfmtIJRIm6nwuEFFhGhYbtThLQPo3CSTtmg

class WebApiKeyValidator {
public:
    virtual ~WebApiKeyValidator() {}

    // Returns true if the given API key is valid for the specified origin and
    // API name.
    virtual bool validateApiKey(const WebString& apiKey, const WebString& origin, const WebString& apiName) = 0;
};

} // namespace blink

#endif // WebApiKeyValidator_h
