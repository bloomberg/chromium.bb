// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_CUSTOM_SCALING_H_
#define PRINTING_CUSTOM_SCALING_H_

#include "printing/printing_export.h"

namespace printing {

// This set of function allows plugin to set custom printing scale in the
// thread local storage. It is a hack to pass scale through this, but
// alternative is to do major refactoring to pass this scale factor all the
// from plugin through webkit to the upper level.
// We should definitely revisit this approach in favor of better implementation
// later. In the mean time, this looks like a simplest way fixing printing
// issues now.

// Gets custom printing scale from the TLS. Return false if it has not been
// set.
PRINTING_EXPORT bool GetCustomPrintingPageScale(double* scale);

// Sets custom printing scale in TLS.
PRINTING_EXPORT void SetCustomPrintingPageScale(double scale);

// Clears custom printing scale in TLS.
PRINTING_EXPORT void ClearCustomPrintingPageScale();

}  // namespace printing

#endif  // PRINTING_CUSTOM_SCALING_H_

