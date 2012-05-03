// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/custom_scaling.h"

#include "base/threading/thread_local.h"

namespace printing {

static base::ThreadLocalPointer<double> gPrintingPageScale;

bool GetCustomPrintingPageScale(double* scale) {
  double* ptr = gPrintingPageScale.Get();
  if (ptr != NULL) {
    *scale = *ptr;
  }
  return ptr != NULL;
}

void SetCustomPrintingPageScale(double scale) {
  ClearCustomPrintingPageScale();
  double* ptr = new double;
  *ptr = scale;
  gPrintingPageScale.Set(ptr);
}

void ClearCustomPrintingPageScale() {
  double* ptr = gPrintingPageScale.Get();
  if (ptr != NULL) {
    delete ptr;
  }
  gPrintingPageScale.Set(NULL);
}

}  // namespace printing

