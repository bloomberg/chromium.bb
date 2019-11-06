// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep.h"

class AppServiceProxyImplTest : public testing::Test {
 protected:
  using UniqueReleaser = std::unique_ptr<apps::IconLoader::Releaser>;

  class FakeIconLoader : public apps::IconLoader {
   public:
    void FlushPendingCallbacks() {
      for (auto& callback : pending_callbacks_) {
        auto iv = apps::mojom::IconValue::New();
        iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
        iv->uncompressed =
            gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
        iv->is_placeholder_icon = false;

        std::move(callback).Run(std::move(iv));
        num_inner_finished_callbacks_++;
      }
      pending_callbacks_.clear();
    }

    int NumInnerFinishedCallbacks() { return num_inner_finished_callbacks_; }
    int NumPendingCallbacks() { return pending_callbacks_.size(); }

   private:
    apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override {
      return apps::mojom::IconKey::New(0, 0, 0);
    }

    std::unique_ptr<Releaser> LoadIconFromIconKey(
        apps::mojom::AppType app_type,
        const std::string& app_id,
        apps::mojom::IconKeyPtr icon_key,
        apps::mojom::IconCompression icon_compression,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::mojom::Publisher::LoadIconCallback callback) override {
      if (icon_compression == apps::mojom::IconCompression::kUncompressed) {
        pending_callbacks_.push_back(std::move(callback));
      }
      return nullptr;
    }

    int num_inner_finished_callbacks_ = 0;
    std::vector<apps::mojom::Publisher::LoadIconCallback> pending_callbacks_;
  };

  UniqueReleaser LoadIcon(apps::IconLoader* loader, const std::string& app_id) {
    static constexpr auto app_type = apps::mojom::AppType::kWeb;
    static constexpr auto icon_compression =
        apps::mojom::IconCompression::kUncompressed;
    static constexpr int32_t size_hint_in_dip = 1;
    static bool allow_placeholder_icon = false;

    return loader->LoadIcon(app_type, app_id, icon_compression,
                            size_hint_in_dip, allow_placeholder_icon,
                            base::BindOnce(&AppServiceProxyImplTest::OnLoadIcon,
                                           base::Unretained(this)));
  }

  void OverrideAppServiceProxyInnerIconLoader(apps::AppServiceProxyImpl* proxy,
                                              apps::IconLoader* icon_loader) {
    proxy->OverrideInnerIconLoaderForTesting(icon_loader);
  }

  void OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
    num_outer_finished_callbacks_++;
  }

  int NumOuterFinishedCallbacks() { return num_outer_finished_callbacks_; }

  int num_outer_finished_callbacks_ = 0;
};

TEST_F(AppServiceProxyImplTest, IconCache) {
  apps::AppServiceProxyImpl proxy(nullptr);
  FakeIconLoader fake;
  OverrideAppServiceProxyInnerIconLoader(&proxy, &fake);

  // The next LoadIcon call should be a cache miss.
  UniqueReleaser c0 = LoadIcon(&proxy, "cromulent");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(0, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(0, NumOuterFinishedCallbacks());

  // After a cache miss, manually trigger the inner callback.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(1, NumOuterFinishedCallbacks());

  // The next LoadIcon call should be a cache hit.
  UniqueReleaser c1 = LoadIcon(&proxy, "cromulent");
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(2, NumOuterFinishedCallbacks());

  // Destroy the IconLoader::Releaser's, clearing the cache.
  c0.reset();
  c1.reset();

  // The next LoadIcon call should be a cache miss.
  UniqueReleaser c2 = LoadIcon(&proxy, "cromulent");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(2, NumOuterFinishedCallbacks());

  // After a cache miss, manually trigger the inner callback.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(3, NumOuterFinishedCallbacks());
}

// TODO(crbug.com/826982): test coalescing multiple in-flight calls with the
// same IconLoader::Key.
