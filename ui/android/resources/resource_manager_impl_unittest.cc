// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/ui_resource_bitmap.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/resources/system_ui_resource_type.h"
#include "ui/android/resources/ui_resource_client_android.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/gfx/android/java_bitmap.h"

namespace ui {

class TestResourceManagerImpl : public ResourceManagerImpl {
 public:
  explicit TestResourceManagerImpl(UIResourceProvider* provider)
      : ResourceManagerImpl(provider) {}

  ~TestResourceManagerImpl() override {}

  void SetResourceAsLoaded(AndroidResourceType res_type, int res_id) {
    SkBitmap small_bitmap;
    SkCanvas canvas(small_bitmap);
    small_bitmap.allocPixels(
        SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
    canvas.drawColor(SK_ColorWHITE);
    small_bitmap.setImmutable();

    OnResourceReady(NULL, NULL, res_type, res_id,
                    gfx::ConvertToJavaBitmap(&small_bitmap).obj(), 0, 0, 0, 0,
                    0, 0, 0, 0);
  }

 protected:
  void PreloadResourceFromJava(AndroidResourceType res_type,
                               int res_id) override {}

  void RequestResourceFromJava(AndroidResourceType res_type,
                               int res_id) override {
    SetResourceAsLoaded(res_type, res_id);
  }
};

namespace {

const ui::SystemUIResourceType kTestResourceType = ui::OVERSCROLL_GLOW;

class MockUIResourceProvider : public ui::UIResourceProvider {
 public:
  MockUIResourceProvider()
      : next_ui_resource_id_(1),
        has_layer_tree_host_(true),
        resource_manager_(this) {}

  virtual ~MockUIResourceProvider() {}

  cc::UIResourceId CreateUIResource(
      ui::UIResourceClientAndroid* client) override {
    if (!has_layer_tree_host_)
      return 0;
    cc::UIResourceId id = next_ui_resource_id_++;
    client->GetBitmap(id, false);
    ui_resource_client_map_[id] = client;
    return id;
  }

  void DeleteUIResource(cc::UIResourceId id) override {
    CHECK(has_layer_tree_host_);
    ui_resource_client_map_.erase(id);
  }

  bool SupportsETC1NonPowerOfTwo() const override { return true; }

  void LayerTreeHostCleared() {
    has_layer_tree_host_ = false;
    UIResourceClientMap client_map = ui_resource_client_map_;
    ui_resource_client_map_.clear();
    for (UIResourceClientMap::iterator iter = client_map.begin();
         iter != client_map.end(); iter++) {
      iter->second->UIResourceIsInvalid();
    }
  }

  void LayerTreeHostReturned() { has_layer_tree_host_ = true; }

  TestResourceManagerImpl& GetResourceManager() { return resource_manager_; }

  cc::UIResourceId next_ui_resource_id() const { return next_ui_resource_id_; }

 private:
  typedef base::hash_map<cc::UIResourceId, ui::UIResourceClientAndroid*>
      UIResourceClientMap;

  cc::UIResourceId next_ui_resource_id_;
  UIResourceClientMap ui_resource_client_map_;
  bool has_layer_tree_host_;

  // The UIResourceProvider owns the ResourceManager.
  TestResourceManagerImpl resource_manager_;
};

}  // namespace

class ResourceManagerTest : public testing::Test {
 public:
  void PreloadResource(ui::SystemUIResourceType type) {
    ui_resource_provider_.GetResourceManager().PreloadResource(
        ui::ANDROID_RESOURCE_TYPE_SYSTEM, type);
  }

  cc::UIResourceId GetUIResourceId(ui::SystemUIResourceType type) {
    return ui_resource_provider_.GetResourceManager().GetUIResourceId(
        ui::ANDROID_RESOURCE_TYPE_SYSTEM, type);
  }

  void LayerTreeHostCleared() { ui_resource_provider_.LayerTreeHostCleared(); }

  void LayerTreeHostReturned() {
    ui_resource_provider_.LayerTreeHostReturned();
  }

  void SetResourceAsLoaded(ui::SystemUIResourceType type) {
    ui_resource_provider_.GetResourceManager().SetResourceAsLoaded(
        ui::ANDROID_RESOURCE_TYPE_SYSTEM, type);
  }

  cc::UIResourceId GetNextUIResourceId() const {
    return ui_resource_provider_.next_ui_resource_id();
  }

 private:
  MockUIResourceProvider ui_resource_provider_;
};

TEST_F(ResourceManagerTest, GetResource) {
  EXPECT_NE(0, GetUIResourceId(kTestResourceType));
}

TEST_F(ResourceManagerTest, PreloadEnsureResource) {
  // Preloading the resource should trigger bitmap loading, but the actual
  // resource id will not be generated until it is explicitly requested.
  cc::UIResourceId first_resource_id = GetNextUIResourceId();
  PreloadResource(kTestResourceType);
  SetResourceAsLoaded(kTestResourceType);
  EXPECT_EQ(first_resource_id, GetNextUIResourceId());
  EXPECT_NE(0, GetUIResourceId(kTestResourceType));
  EXPECT_NE(first_resource_id, GetNextUIResourceId());
}

TEST_F(ResourceManagerTest, ResetLayerTreeHost) {
  EXPECT_NE(0, GetUIResourceId(kTestResourceType));
  LayerTreeHostCleared();
  EXPECT_EQ(0, GetUIResourceId(kTestResourceType));
  LayerTreeHostReturned();
  EXPECT_NE(0, GetUIResourceId(kTestResourceType));
}

}  // namespace ui
