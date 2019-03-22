// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_features.h"

namespace chrome_pdf {
namespace features {

const base::Feature kSaveEditedPDFForm{"SaveEditedPDFForm",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPDFAnnotations{"PDFAnnotations",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace chrome_pdf
