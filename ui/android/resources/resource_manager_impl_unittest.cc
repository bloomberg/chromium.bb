// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/ui_resource_bitmap.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/resources/system_ui_resource_type.h"
#include "ui/gfx/android/java_bitmap.h"


using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MatcherCast;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace ui {

class TestResourceManagerImpl : public ResourceManagerImpl {
 public:
  TestResourceManagerImpl() {}
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

class MockLayerTreeHost : public cc::LayerTreeHost {
 public:
  MockLayerTreeHost(cc::LayerTreeHost::InitParams* params)
      : cc::LayerTreeHost(params) {}

  MOCK_METHOD1(CreateUIResource, cc::UIResourceId(cc::UIResourceClient*));
  MOCK_METHOD1(DeleteUIResource, void(cc::UIResourceId));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLayerTreeHost);
};

}  // namespace

class ResourceManagerTest : public testing::Test {
 public:
  ResourceManagerTest() : fake_client_(cc::FakeLayerTreeHostClient::DIRECT_3D) {
    cc::LayerTreeHost::InitParams params;
    cc::LayerTreeSettings settings;
    params.client = &fake_client_;
    params.settings = &settings;
    params.task_graph_runner = &task_graph_runner_;
    host_.reset(new MockLayerTreeHost(&params));
    resource_manager_.Init(host_.get());
  }

  void PreloadResource(ui::SystemUIResourceType type) {
    resource_manager_.PreloadResource(ui::ANDROID_RESOURCE_TYPE_SYSTEM, type);
  }

  cc::UIResourceId GetUIResourceId(ui::SystemUIResourceType type) {
    return resource_manager_.GetUIResourceId(ui::ANDROID_RESOURCE_TYPE_SYSTEM,
                                             type);
  }

  void SetResourceAsLoaded(ui::SystemUIResourceType type) {
    resource_manager_.SetResourceAsLoaded(ui::ANDROID_RESOURCE_TYPE_SYSTEM,
                                          type);
  }

 protected:
  scoped_ptr<MockLayerTreeHost> host_;
  TestResourceManagerImpl resource_manager_;
  cc::TestTaskGraphRunner task_graph_runner_;
  cc::FakeLayerTreeHostClient fake_client_;
};

TEST_F(ResourceManagerTest, GetResource) {
  const cc::UIResourceId kResourceId = 99;
  EXPECT_CALL(*host_.get(), CreateUIResource(_))
      .WillOnce(Return(kResourceId))
      .RetiresOnSaturation();
  EXPECT_EQ(kResourceId, GetUIResourceId(kTestResourceType));
}

TEST_F(ResourceManagerTest, PreloadEnsureResource) {
  const cc::UIResourceId kResourceId = 99;
  PreloadResource(kTestResourceType);
  EXPECT_CALL(*host_.get(), CreateUIResource(_))
      .WillOnce(Return(kResourceId))
      .RetiresOnSaturation();
  SetResourceAsLoaded(kTestResourceType);
  EXPECT_EQ(kResourceId, GetUIResourceId(kTestResourceType));
}

}  // namespace ui
