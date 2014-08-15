// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ANDROID_SYSTEM_UI_RESOURCE_MANAGER_H_
#define UI_BASE_ANDROID_SYSTEM_UI_RESOURCE_MANAGER_H_

#include "cc/resources/ui_resource_client.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// Interface for loading and accessing shared system UI resources.
class UI_BASE_EXPORT SystemUIResourceManager {
 public:
  enum ResourceType {
    OVERSCROLL_EDGE = 0,
    OVERSCROLL_GLOW,
    OVERSCROLL_GLOW_L,
    RESOURCE_TYPE_FIRST = OVERSCROLL_EDGE,
    RESOURCE_TYPE_LAST = OVERSCROLL_GLOW_L
  };

  virtual ~SystemUIResourceManager() {}

  // Optionally trigger bitmap loading for a given |resource|, if necessary.
  // Note that this operation may be asynchronous, and subsequent queries to
  // |GetUIResourceId()| will yield a valid result only after loading finishes.
  // This method is particularly useful for idly loading a resource before an
  // explicit cc::UIResourceId is required. Repeated calls will be ignored.
  virtual void PreloadResource(ResourceType resource) = 0;

  // Return the resource id associated with |resource|. If loading hasn't yet
  // begun for the given |resource|, it will be triggered immediately. If
  // loading is asynchronous, 0 will be returned until loading has finished, and
  // the caller is responsible for re-querying until a valid id is returned.
  virtual cc::UIResourceId GetUIResourceId(ResourceType resource) = 0;
};

}  // namespace ui

#endif  // UI_BASE_ANDROID_SYSTEM_UI_RESOURCE_MANAGER_H_
