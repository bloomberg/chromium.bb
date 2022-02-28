// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_

#include "base/time/time.h"
#include "url/origin.h"

namespace subresource_redirect {

// Returns the timeout for the compressed subresource redirect, after which the
// subresource should be fetched directly from the origin.
base::TimeDelta GetCompressionRedirectTimeout();

// Returns the public image hinte receive timeout value from field trial.
int64_t GetHintsReceiveTimeout();

// Returns the timeout to wait for the robots rules to be received, after which
// the subresource should be fetched directly from the origin.
base::TimeDelta GetRobotsRulesReceiveTimeout();

// Returns the count of subresources for which first k timeout limit should be
// applied.
size_t GetFirstKSubresourceLimit();

// Returns the timeout for first k subresouces, to wait for the robots rules to
// be received, after which the subresource should be fetched directly from the
// origin.
base::TimeDelta GetRobotsRulesReceiveFirstKSubresourceTimeout();

// Returns the count of first k subresources for which redirect compression
// should be disabled. Value of 0 means this first k subresource disabling
// feature is turned off.
size_t GetFirstKDisableSubresourceRedirectLimit();

// The maximum number of robots rules parsers the renderer should cache locally
// for reuse by the renderframes in the renderer process.
int MaxRobotsRulesParsersCacheSize();

// Returns whether image compression ukm metrics should be recorded.
bool ShouldRecordLoginRobotsUkmMetrics();

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_PARAMS_H_
