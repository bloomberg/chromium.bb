// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_URL_CONSTANTS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_URL_CONSTANTS_H_

namespace dom_distiller {


#if defined(USE_DOM_DISTILLER)
extern const char kDomDistillerScheme[];
#endif

extern const char kEntryIdKey[];
extern const char kUrlKey[];
extern const char kTimeKey[];
extern const char kViewerCssPath[];
extern const char kViewerLoadingImagePath[];
extern const char kViewerSaveFontScalingPath[];

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_URL_CONSTANTS_H_
