// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/TransformPrinters.h"

#include <ostream>  // NOLINT
#include "platform/transforms/AffineTransform.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

void PrintTo(const AffineTransform& transform, std::ostream* os) {
  *os << transform.ToString();
}

void PrintTo(const TransformationMatrix& matrix, std::ostream* os) {
  *os << matrix.ToString();
}

}  // namespace blink
