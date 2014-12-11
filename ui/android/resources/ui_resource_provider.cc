// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/ui_resource_provider.h"

#include "cc/resources/ui_resource_client.h"
#include "cc/trees/layer_tree_host.h"
#include "ui/android/resources/ui_resource_client_android.h"

namespace ui {

UIResourceProvider::UIResourceProvider()
    : host_(NULL), supports_etc1_npot_(false) {
}

UIResourceProvider::~UIResourceProvider() {
  SetLayerTreeHost(NULL);
}

void UIResourceProvider::SetLayerTreeHost(cc::LayerTreeHost* host) {
  if (host_ == host)
    return;
  host_ = host;
  UIResourcesAreInvalid();
}

void UIResourceProvider::UIResourcesAreInvalid() {
  UIResourceClientMap client_map;
  client_map.swap(ui_resource_client_map_);
  for (UIResourceClientMap::iterator iter = client_map.begin();
       iter != client_map.end(); iter++) {
    iter->second->UIResourceIsInvalid();
  }
}

cc::UIResourceId UIResourceProvider::CreateUIResource(
    ui::UIResourceClientAndroid* client) {
  if (!host_)
    return 0;
  cc::UIResourceId id = host_->CreateUIResource(client);
  DCHECK(ui_resource_client_map_.find(id) == ui_resource_client_map_.end());

  ui_resource_client_map_[id] = client;
  return id;
}

void UIResourceProvider::DeleteUIResource(cc::UIResourceId ui_resource_id) {
  UIResourceClientMap::iterator iter =
      ui_resource_client_map_.find(ui_resource_id);
  DCHECK(iter != ui_resource_client_map_.end());

  ui_resource_client_map_.erase(iter);

  if (!host_)
    return;
  host_->DeleteUIResource(ui_resource_id);
}

bool UIResourceProvider::SupportsETC1NonPowerOfTwo() const {
  return supports_etc1_npot_;
}

}  // namespace ui
