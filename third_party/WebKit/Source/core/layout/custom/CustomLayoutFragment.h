// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomLayoutFragment_h
#define CustomLayoutFragment_h

#include "platform/LayoutUnit.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CustomLayoutFragmentRequest;
class LayoutBox;

// This represents the result of a layout (on a LayoutChild).
//
// The result is stuck in time, e.g. performing another layout of the same
// LayoutChild will not live update the values of this fragment.
//
// The web developer can position this child fragment (setting inlineOffset,
// and blockOffset), which are relative to its parent.
//
// This should eventually mirror the information in a NGFragment, it has the
// additional capability that it is exposed to web developers.
class CustomLayoutFragment : public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(CustomLayoutFragment);
  DEFINE_WRAPPERTYPEINFO();

 public:
  CustomLayoutFragment(CustomLayoutFragmentRequest*,
                       const LayoutUnit inline_size,
                       const LayoutUnit block_size);
  virtual ~CustomLayoutFragment() = default;

  double inlineSize() const { return inline_size_; }
  double blockSize() const { return block_size_; }

  LayoutBox* GetLayoutBox() const;
  bool IsValid() const;

  void Trace(blink::Visitor*) override;

 private:
  // There is complexity around state in the layout tree, e.g. from the web
  // developers perspective:
  //
  // const fragment1 = yield child.layoutNextFragment(options1);
  // const fragment2 = yield child.layoutNextFragment(options2);
  // return { childFragments: [fragment1] };
  //
  // In the above example the LayoutBox representing "child" has the incorrect
  // layout state. As we are processing the returned childFragments we detect
  // that the last layout on the child wasn't with the same inputs, and force a
  // layout again.
  Member<CustomLayoutFragmentRequest> fragment_request_;

  // The inline and block size on this object should never change.
  const double inline_size_;
  const double block_size_;
};

}  // namespace blink

#endif  // CustomLayoutFragment_h
