// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_UNITS_H_
#define PRINTING_UNITS_H_

#include "printing/printing_export.h"

namespace printing {

// Length of an inch in 0.001mm unit.
constexpr int kMicronsPerInch = 25400;

// Mil is a thousandth of an inch.
constexpr float kMicronsPerMil = 25.4f;
constexpr int kMilsPerInch = 1000;

// Length of an inch in CSS's 1pt unit.
// http://dev.w3.org/csswg/css3-values/#absolute-length-units-cm-mm.-in-pt-pc
constexpr int kPointsPerInch = 72;

// Length of an inch in CSS's 1px unit.
// http://dev.w3.org/csswg/css3-values/#the-px-unit
constexpr int kPixelsPerInch = 96;

// Dpi used to save to PDF or Cloud Print.
constexpr int kDefaultPdfDpi = 300;

// LETTER: 8.5 x 11 inches
constexpr float kLetterWidthInch = 8.5f;
constexpr float kLetterHeightInch = 11.0f;

// LEGAL: 8.5 x 14 inches
constexpr float kLegalWidthInch = 8.5f;
constexpr float kLegalHeightInch = 14.0f;

// A4: 8.27 x 11.69 inches
constexpr float kA4WidthInch = 8.27f;
constexpr float kA4HeightInch = 11.69f;

// A3: 11.69 x 16.54 inches
constexpr float kA3WidthInch = 11.69f;
constexpr float kA3HeightInch = 16.54f;

// Converts from one unit system to another using integer arithmetics.
PRINTING_EXPORT int ConvertUnit(double value, int old_unit, int new_unit);

// Converts from one unit system to another using doubles.
PRINTING_EXPORT double ConvertUnitDouble(double value,
                                         double old_unit,
                                         double new_unit);

// Converts from 1 pixel to 1 point using integers.
PRINTING_EXPORT int ConvertPixelsToPoint(int pixels);

// Converts from 1 pixel to 1 point using doubles.
PRINTING_EXPORT double ConvertPixelsToPointDouble(double pixels);

// Converts from 1 point to 1 pixel using doubles.
PRINTING_EXPORT double ConvertPointsToPixelDouble(double points);

}  // namespace printing

#endif  // PRINTING_UNITS_H_
