// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomLayoutFragmentRequest_h
#define CustomLayoutFragmentRequest_h

#include "core/layout/custom/CustomLayoutConstraintsOptions.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CustomLayoutChild;
class CustomLayoutFragment;
class LayoutBox;

// This represents a request to perform layout on a child. It is an opaque
// object from the web developers point of view.
class CustomLayoutFragmentRequest : public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(CustomLayoutFragmentRequest);
  DEFINE_WRAPPERTYPEINFO();

 public:
  CustomLayoutFragmentRequest(CustomLayoutChild*,
                              const CustomLayoutConstraintsOptions&);
  virtual ~CustomLayoutFragmentRequest() = default;

  // Produces a CustomLayoutFragment from this CustomLayoutFragmentRequest. This
  // may fail if the underlying LayoutBox represented by the CustomLayoutChild
  // has been removed from the tree.
  CustomLayoutFragment* PerformLayout();

  LayoutBox* GetLayoutBox() const;
  bool IsValid() const;

  void Trace(blink::Visitor*) override;

 private:
  Member<CustomLayoutChild> child_;
  const CustomLayoutConstraintsOptions options_;
};

}  // namespace blink

#endif  // CustomLayoutFragmentRequest_h
