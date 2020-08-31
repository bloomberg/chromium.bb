// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the pdf
// module.

#ifndef PDF_PDF_FEATURES_H_
#define PDF_PDF_FEATURES_H_

#include "base/feature_list.h"

namespace chrome_pdf {
namespace features {

extern const base::Feature kAccessiblePDFForm;
extern const base::Feature kAccessiblePDFHighlight;
extern const base::Feature kPDFAnnotations;
extern const base::Feature kPDFTwoUpView;
extern const base::Feature kSaveEditedPDFForm;
extern const base::Feature kTabAcrossPDFAnnotations;

}  // namespace features
}  // namespace chrome_pdf

#endif  // PDF_PDF_FEATURES_H_
