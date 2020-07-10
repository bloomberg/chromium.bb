// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/url_constants.h"

namespace dom_distiller {
#if defined(USE_DOM_DISTILLER)
const char kDomDistillerScheme[] = "chrome-distiller";
#endif

const char kEntryIdKey[] = "entry_id";
const char kUrlKey[] = "url";
const char kTimeKey[] = "time";
const char kViewerCssPath[] = "dom_distiller_viewer.css";
const char kViewerLoadingImagePath[] = "dom_distiller_material_spinner.svg";
const char kViewerSaveFontScalingPath[] = "savefontscaling/";

}  // namespace dom_distiller
