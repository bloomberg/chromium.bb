// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeProperties_h
#define ShapeProperties_h

namespace blink {

// Bit field values indicating available display shapes.
enum DisplayShape {
  DisplayShapeRect = 1 << 0,
  DisplayShapeRound = 1 << 1,
};

}  // namespace blink

#endif
