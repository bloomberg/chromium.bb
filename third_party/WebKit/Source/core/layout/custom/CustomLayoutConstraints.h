// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomLayoutConstraints_h
#define CustomLayoutConstraints_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

// Represents the constraints given to the layout by the parent that isn't
// encapsulated by the style, or edges.
class CustomLayoutConstraints : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CustomLayoutConstraints(LayoutUnit fixed_inline_size)
      : fixed_inline_size_(fixed_inline_size.ToDouble()) {}
  virtual ~CustomLayoutConstraints() = default;

  // LayoutConstraints.idl
  double fixedInlineSize() const { return fixed_inline_size_; }

 private:
  double fixed_inline_size_;

  DISALLOW_COPY_AND_ASSIGN(CustomLayoutConstraints);
};

}  // namespace blink

#endif  // CustomLayoutConstraints_h
