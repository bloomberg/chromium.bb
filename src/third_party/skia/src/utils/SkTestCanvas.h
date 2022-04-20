/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// SkTestCanvas is a simple way to make a testing canvas which is allowed to use private
// facilities of SkCanvas without having to add a friend to SkCanvas.h.
//
// You create a Key (a simple empty struct) to make a template specialization class. You need to
// make a key for each of the different Canvases you need. The implementations of the canvases
// are in SkCanvas.cpp, which allows the use of helper classes.

#ifndef SkTestCanvas_DEFINED
#define SkTestCanvas_DEFINED

#include "include/core/SkSize.h"
#include "include/utils/SkNWayCanvas.h"
#include "src/core/SkDevice.h"
#include "src/core/SkGlyphRun.h"

// You can only make template specializations of SkTestCanvas.
template <typename Key> class SkTestCanvas;

// A test canvas to test using slug rendering instead of text blob rendering.
struct SkSlugTestKey {};
template <>
class SkTestCanvas<SkSlugTestKey> : public SkCanvas {
public:
    SkTestCanvas(SkCanvas* canvas);
    void onDrawGlyphRunList(const SkGlyphRunList& glyphRunList, const SkPaint& paint) override;
};

#endif  // SkTestCanvas_DEFINED
