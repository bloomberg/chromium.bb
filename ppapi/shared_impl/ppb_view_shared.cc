// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_view_shared.h"

namespace ppapi {

ViewData::ViewData() {
  // Assume POD.
  memset(this, 0, sizeof(ViewData));
}

ViewData::~ViewData() {
}

bool ViewData::Equals(const ViewData& other) const {
  return rect.point.x == other.rect.point.x &&
         rect.point.y == other.rect.point.y &&
         rect.size.width == other.rect.size.width &&
         rect.size.height == other.rect.size.height &&
         is_fullscreen == other.is_fullscreen &&
         is_page_visible == other.is_page_visible &&
         clip_rect.point.x == other.clip_rect.point.x &&
         clip_rect.point.y == other.clip_rect.point.y &&
         clip_rect.size.width == other.clip_rect.size.width &&
         clip_rect.size.height == other.clip_rect.size.height &&
         device_scale == other.device_scale &&
         css_scale == other.css_scale;
}

PPB_View_Shared::PPB_View_Shared(ResourceObjectType type,
                                 PP_Instance instance,
                                 const ViewData& data)
    : Resource(type, instance),
      data_(data) {
}

PPB_View_Shared::~PPB_View_Shared() {
}

thunk::PPB_View_API* PPB_View_Shared::AsPPB_View_API() {
  return this;
}

const ViewData& PPB_View_Shared::GetData() const {
  return data_;
}

}  // namespace ppapi
