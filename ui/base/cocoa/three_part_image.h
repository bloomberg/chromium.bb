// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_THREE_PART_IMAGE_H
#define UI_BASE_COCOA_THREE_PART_IMAGE_H

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// A wrapper around NSDrawThreePartImage that caches the images.
// The middle image is optional.
// Vertical orientation is not currently supported.
class UI_BASE_EXPORT ThreePartImage {
 public:
  // |left_id|, |middle_id|, and |right_id| are ResourceBundle image
  // identifiers. Specify 0 for |middle_id| if there is no middle image.
  ThreePartImage(int left_id, int middle_id, int right_id);
  ~ThreePartImage();

  // Returns the image rects if drawn in |bounds|.
  NSRect GetLeftRect(NSRect bounds) const;
  NSRect GetMiddleRect(NSRect bounds) const;
  NSRect GetRightRect(NSRect bounds) const;

  // Draws the three part image inside |rect|.
  void DrawInRect(NSRect rect, NSCompositingOperation op, CGFloat alpha) const;

  // Returns YES if |point| is in a non-transparent part of the images.
  // Returns YES if |point| is inside the middle rect and there is no middle
  // image.
  BOOL HitTest(NSPoint point, NSRect bounds) const;

 private:
  // Returns YES if |point| is in a non-transparent part of |image|.
  BOOL HitTestImage(NSPoint point, NSImage* image, NSRect imageRect) const;

  base::scoped_nsobject<NSImage> leftImage_;
  base::scoped_nsobject<NSImage> middleImage_;
  base::scoped_nsobject<NSImage> rightImage_;
  NSSize leftSize_;
  NSSize rightSize_;

  DISALLOW_COPY_AND_ASSIGN(ThreePartImage);
};

}  // namespace ui

#endif  // UI_BASE_COCOA_THREE_PART_IMAGE_H
