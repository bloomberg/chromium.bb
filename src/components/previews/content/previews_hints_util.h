// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_UTIL_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_UTIL_H_

class GURL;
namespace optimization_guide {
namespace proto {
class Hint;
class Optimization;
class PageHint;
}  // namespace proto
}  // namespace optimization_guide

namespace previews {

// Returns whether |optimization| is disabled subject to it being part of
// an optimization hint experiment. |optimization| could be disabled either
// because: it is only to be used with a named optimization experiment; or it
// is not to be used with a named excluded experiment. One experiment name
// may be configured for the client with the experiment_name parameter to the
// kOptimizationHintsExperiments feature.
bool IsDisabledPerOptimizationHintExperiment(
    const optimization_guide::proto::Optimization& optimization);

// Returns the matching PageHint for |gurl| if found in |hint|.
const optimization_guide::proto::PageHint* FindPageHintForURL(
    const GURL& gurl,
    const optimization_guide::proto::Hint* hint);

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_UTIL_H_
