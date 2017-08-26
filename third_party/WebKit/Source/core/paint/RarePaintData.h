// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RarePaintData_h
#define RarePaintData_h

#include "core/CoreExport.h"
#include "core/paint/FragmentData.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PaintLayer;

// This is for paint-related data on LayoutObject that is not needed on all
// objects.
class CORE_EXPORT RarePaintData {
  WTF_MAKE_NONCOPYABLE(RarePaintData);
  USING_FAST_MALLOC(RarePaintData);

 public:
  RarePaintData();
  ~RarePaintData();

  PaintLayer* Layer() { return layer_.get(); }
  void SetLayer(std::unique_ptr<PaintLayer>);

  FragmentData* Fragment() const { return fragment_data_.get(); }

  FragmentData& EnsureFragment();

  // An id for this object that is unique for the lifetime of the WebView.
  UniqueObjectId UniqueId() const { return unique_id_; }

 private:
  // The PaintLayer associated with this LayoutBoxModelObject. This can be null
  // depending on the return value of LayoutBoxModelObject::layerTypeRequired().
  std::unique_ptr<PaintLayer> layer_;

  std::unique_ptr<FragmentData> fragment_data_;

  UniqueObjectId unique_id_;
};

}  // namespace blink

#endif
