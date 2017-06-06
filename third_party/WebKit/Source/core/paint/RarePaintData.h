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

class PropertyTreeState;
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

  ObjectPaintProperties* PaintProperties() const {
    if (fragment_data_)
      return fragment_data_->PaintProperties();
    return nullptr;
  }

  PropertyTreeState* LocalBorderBoxProperties() const {
    return local_border_box_properties_.get();
  }

  void ClearLocalBorderBoxProperties();
  void SetLocalBorderBoxProperties(PropertyTreeState&);

  // This is the complete set of property nodes that can be used to paint the
  // contents of this object. It is similar to local_border_box_properties_ but
  // includes properties (e.g., overflow clip, scroll translation) that apply
  // to contents.
  PropertyTreeState ContentsProperties() const;

  // An id for this object that is unique for the lifetime of the WebView.
  LayoutObjectId UniqueId() const { return unique_id_; }

 private:
  // The PaintLayer associated with this LayoutBoxModelObject. This can be null
  // depending on the return value of LayoutBoxModelObject::layerTypeRequired().
  std::unique_ptr<PaintLayer> layer_;

  std::unique_ptr<FragmentData> fragment_data_;

  // This is a complete set of property nodes that should be used as a
  // starting point to paint a LayoutObject. This data is cached because some
  // properties inherit from the containing block chain instead of the
  // painting parent and cannot be derived in O(1) during the paint walk.
  //
  // For example: <div style='opacity: 0.3;'/>
  //   The div's local border box properties would have an opacity 0.3 effect
  //   node. Even though the div has no transform, its local border box
  //   properties would have a transform node that points to the div's
  //   ancestor transform space.
  std::unique_ptr<PropertyTreeState> local_border_box_properties_;

  LayoutObjectId unique_id_;
};

}  // namespace blink

#endif
