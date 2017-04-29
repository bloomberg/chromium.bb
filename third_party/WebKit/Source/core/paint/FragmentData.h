// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FragmentData_h
#define FragmentData_h

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

class ObjectPaintProperties;

// Represents the data for a particular fragment of a LayoutObject.
// Only LayoutObjects with a self-painting PaintLayer may have more than one
// FragmentDeta, and even then only when they are inside of multicol.
// See README.md.
class CORE_EXPORT FragmentData {
 public:
  static std::unique_ptr<FragmentData> Create() {
    return WTF::WrapUnique(new FragmentData());
  }

  ObjectPaintProperties* PaintProperties() const {
    return paint_properties_.get();
  }
  ObjectPaintProperties& EnsurePaintProperties();
  void ClearPaintProperties();

  FragmentData* NextFragment() { return next_fragment_.get(); }

 private:
  // Holds references to the paint property nodes created by this object.
  std::unique_ptr<ObjectPaintProperties> paint_properties_;

  std::unique_ptr<FragmentData> next_fragment_;
};

}  // namespace blink

#endif  // FragmentData_h
