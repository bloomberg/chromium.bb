// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FEATURES_H_
#define NET_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "net/base/net_export.h"

namespace net {
namespace features {

// Uses a site isolated code cache that is keyed on the resource url and the
// origin lock of the renderer that is requesting the resource. The requests
// to site-isolated code cache are handled by the content/GeneratedCodeCache
// When this flag is enabled, the metadata field of the HttpCache is unused.
NET_EXPORT extern const base::Feature kIsolatedCodeCache;

// Enables the additional TLS 1.3 server-random-based downgrade protection
// described in https://tools.ietf.org/html/rfc8446#section-4.1.3
//
// This is a MUST-level requirement of TLS 1.3, but has compatibility issues
// with some buggy non-compliant TLS-terminating proxies.
NET_EXPORT extern const base::Feature kEnforceTLS13Downgrade;

// Enables optimizing the network quality estimation algorithms in network
// quality estimator (NQE).
NET_EXPORT extern const base::Feature kNetworkQualityEstimator;

// Splits cache entries by the request's top frame's origin if one is available.
NET_EXPORT extern const base::Feature kSplitCacheByTopFrameOrigin;

// Enables sending TLS 1.3 Key Update messages on TLS 1.3 connections in order
// to ensure that this corner of the spec is exercised. This is currently
// disabled by default because we discovered incompatibilities with some
// servers.
NET_EXPORT extern const base::Feature kTLS13KeyUpdate;

// Enables CECPQ2, a post-quantum key-agreement, in TLS 1.3 connections.
NET_EXPORT extern const base::Feature kPostQuantumCECPQ2;

// Changes the timeout after which unused sockets idle sockets are cleaned up.
NET_EXPORT
extern const base::Feature kNetUnusedIdleSocketTimeout;

// When enabled, makes cookies without a SameSite attribute behave like
// SameSite=Lax cookies by default, and requires SameSite=None to be specified
// in order to make cookies available in a third-party context. When disabled,
// the default behavior for cookies without a SameSite attribute specified is no
// restriction, i.e., available in a third-party context.
NET_EXPORT extern const base::Feature kSameSiteByDefaultCookies;

}  // namespace features
}  // namespace net

#endif  // NET_BASE_FEATURES_H_
