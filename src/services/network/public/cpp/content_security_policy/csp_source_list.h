// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"

class GURL;

namespace network {
class CSPContext;

COMPONENT_EXPORT(NETWORK_CPP)
std::string ToString(const mojom::CSPSourceListPtr& source_list);

// Return true when at least one source in the |source_list| matches the
// |url| for a given |context|.
COMPONENT_EXPORT(NETWORK_CPP)
bool CheckCSPSourceList(const mojom::CSPSourceListPtr& source_list,
                        const GURL& url,
                        CSPContext* context,
                        bool has_followed_redirect = false,
                        bool is_response_check = false);

}  // namespace network
#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_
