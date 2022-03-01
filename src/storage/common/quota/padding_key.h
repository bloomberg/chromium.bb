// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_QUOTA_PADDING_KEY_H_
#define STORAGE_COMMON_QUOTA_PADDING_KEY_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "url/gurl.h"

namespace base {
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace storage {

// Utility method to determine if a given type of response should be padded.
COMPONENT_EXPORT(STORAGE_COMMON)
bool ShouldPadResponseType(network::mojom::FetchResponseType type);

// Compute a purely random padding size for a resource.  A random padding is
// preferred except in cases where a site could rapidly trigger a large number
// of padded values for the same resource; e.g. from http cache.
COMPONENT_EXPORT(STORAGE_COMMON)
int64_t ComputeRandomResponsePadding();

// Compute a stable padding value for a resource.  This should be used for
// cases where a site could trigger a large number of padding values to be
// generated for the same resource; e.g. http cache.  The |origin| is the
// origin of the context that loaded the resource.  Note, its important that the
// |response_time| be the time stored in the cache and not just the current
// time.  The |side_data_size| should only be passed if padding is being
// computed for a side data blob.
//
// TODO(https://crbug.com/1199077): Replace `url::Origin` with
// `blink::StorageKey` in the argument list.
COMPONENT_EXPORT(STORAGE_COMMON)
int64_t ComputeStableResponsePadding(const url::Origin& origin,
                                     const std::string& response_url,
                                     const base::Time& response_time,
                                     const std::string& request_method,
                                     int64_t side_data_size = 0);

}  // namespace storage

#endif  // STORAGE_COMMON_QUOTA_PADDING_KEY_H_
