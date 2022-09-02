// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_FEATURES_H_
#define CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_FEATURES_H_

#include "base/feature_list.h"
#include "content/common/content_export.h"

namespace content::features {

// If enabled, then prefetch requests from speculation rules should use the code
// in content/browser/speculation_rules/prefetch/ instead of
// chrome/browser/prefetch/prefetch_proxy/.
extern CONTENT_EXPORT const base::Feature kPrefetchUseContentRefactor;

}  // namespace content::features

#endif  // CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_FEATURES_H_
