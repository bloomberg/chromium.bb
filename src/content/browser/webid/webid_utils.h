// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
#define CONTENT_BROWSER_WEBID_WEBID_UTILS_H_

#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;

// Determines whether |host| is same-origin with all of its ancestors in the
// frame tree. Returns false if not.
// |origin| is provided because it is not considered safe to use
// host->GetLastCommittedOrigin() at some times, so
// DocumentService::origin() should be used to obtain the frame's origin.
bool IsSameOriginWithAncestors(RenderFrameHost* host,
                               const url::Origin& origin);

// Checks requirements for URLs received from the IDP.
bool IdpUrlIsValid(const GURL& url);

}  // namespace content
#endif  // CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
