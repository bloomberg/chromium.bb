// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RarePaintData_h
#define RarePaintData_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PropertyTreeState;
class ObjectPaintProperties;

// This is for paint-related data on LayoutObject that is not needed on all
// objects.
// TODO(pdr): Store LayoutBoxModelObject's m_paintLayer in this structure.
class CORE_EXPORT RarePaintData {
  WTF_MAKE_NONCOPYABLE(RarePaintData);
  USING_FAST_MALLOC(RarePaintData);

 public:
  RarePaintData();
  ~RarePaintData();

  ObjectPaintProperties* PaintProperties() const {
    return paint_properties_.get();
  }
  ObjectPaintProperties& EnsurePaintProperties();

  PropertyTreeState* LocalBorderBoxProperties() const {
    return local_border_box_properties_.get();
  }

  void ClearLocalBorderBoxProperties();
  void SetLocalBorderBoxProperties(PropertyTreeState&);
  const PropertyTreeState* ContentsProperties() const;

 private:
  // Holds references to the paint property nodes created by this object.
  std::unique_ptr<ObjectPaintProperties> paint_properties_;

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

  // This is the complete set of property nodes that can be used to paint the
  // contents of this object. It is similar to m_localBorderBoxProperties but
  // includes properties (e.g., overflow clip, scroll translation) that apply
  // to contents. This cached value is derived from m_localBorderBoxProperties
  // and m_paintProperties and should be invalidated when these change.
  mutable std::unique_ptr<PropertyTreeState> contents_properties_;
};

}  // namespace blink

#endif
