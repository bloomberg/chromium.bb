// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_

#include <string>
#include "base/component_export.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"

class GURL;

namespace network {

class CSPContext;

// Check if a |url| matches with a CSP |source| matches.
COMPONENT_EXPORT(NETWORK_CPP)
bool CheckCSPSource(const mojom::CSPSourcePtr& source,
                    const GURL& url,
                    CSPContext* context,
                    bool has_followed_redirect = false);

// Serialize the CSPSource |source| as a string. This is used for reporting
// violations.
COMPONENT_EXPORT(NETWORK_CPP)
std::string ToString(const mojom::CSPSourcePtr& source);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_
