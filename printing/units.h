// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_UNITS_H_
#define PRINTING_UNITS_H_

#include "printing/printing_export.h"

namespace printing {

// Length of a thousanth of inches in 0.01mm unit.
const int kHundrethsMMPerInch = 2540;

// Length of an inch in CSS's 1pt unit.
// http://dev.w3.org/csswg/css3-values/#absolute-length-units-cm-mm.-in-pt-pc
const int kPointsPerInch = 72;

// Length of an inch in CSS's 1px unit.
// http://dev.w3.org/csswg/css3-values/#the-px-unit
const int kPixelsPerInch = 96;

// Converts from one unit system to another using integer arithmetics.
PRINTING_EXPORT int ConvertUnit(int value, int old_unit, int new_unit);

// Converts from one unit system to another using doubles.
PRINTING_EXPORT double ConvertUnitDouble(double value, double old_unit,
                                         double new_unit);

// Converts from 0.001 inch unit to 0.00001 meter.
PRINTING_EXPORT int ConvertMilliInchToHundredThousanthMeter(int milli_inch);

// Converts from 0.00001 meter unit to 0.001 inch.
PRINTING_EXPORT int ConvertHundredThousanthMeterToMilliInch(int cmm);

// Converts from 1 pixel to 1 point using integers.
PRINTING_EXPORT int ConvertPixelsToPoint(int pixels);

// Converts from 1 pixel to 1 point using doubles.
PRINTING_EXPORT double ConvertPixelsToPointDouble(double pixels);

// Converts from 1 point to 1 pixel using doubles.
double ConvertPointsToPixelDouble(double points);

}  // namespace printing

#endif  // PRINTING_UNITS_H_
