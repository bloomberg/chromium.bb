// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_UI_RESOURCE_PROVIDER_H_
#define UI_ANDROID_RESOURCES_UI_RESOURCE_PROVIDER_H_

#include "base/containers/hash_tables.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/android/ui_android_export.h"

namespace cc {
class LayerTreeHost;
}

namespace ui {

class UIResourceClientAndroid;

class UI_ANDROID_EXPORT UIResourceProvider {
 public:
  UIResourceProvider();
  ~UIResourceProvider();

  void SetLayerTreeHost(cc::LayerTreeHost* host);

  void UIResourcesAreInvalid();

  virtual cc::UIResourceId CreateUIResource(
      ui::UIResourceClientAndroid* client);

  virtual void DeleteUIResource(cc::UIResourceId resource_id);

  void SetSupportsETC1NonPowerOfTwo(bool supports_etc1_npot) {
    supports_etc1_npot_ = supports_etc1_npot;
  }

  virtual bool SupportsETC1NonPowerOfTwo() const;

 private:
  typedef base::hash_map<cc::UIResourceId, ui::UIResourceClientAndroid*>
      UIResourceClientMap;
  UIResourceClientMap ui_resource_client_map_;
  cc::LayerTreeHost* host_;
  bool supports_etc1_npot_;

  DISALLOW_COPY_AND_ASSIGN(UIResourceProvider);
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_UI_RESOURCE_PROVIDER_H_
