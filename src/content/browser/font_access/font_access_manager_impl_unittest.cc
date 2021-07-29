// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/font_access/font_access_test_utils.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::FontEnumerationStatus;

namespace content {

class FontAccessManagerImplTest : public RenderViewHostImplTestHarness {
 public:
  FontAccessManagerImplTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFontAccess);
  }

  void SetUp() override {
    FontEnumerationCache* instance = FontEnumerationCache::GetInstance();
    instance->ResetStateForTesting();

    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    const int process_id = main_rfh()->GetProcess()->GetID();
    const int routing_id = main_rfh()->GetRoutingID();
    const GlobalRenderFrameHostId frame_id =
        GlobalRenderFrameHostId{process_id, routing_id};
    const FontAccessManagerImpl::BindingContext bindingContext = {kTestOrigin,
                                                                  frame_id};

    manager_ = std::make_unique<FontAccessManagerImpl>();
    manager_->BindReceiver(bindingContext,
                           manager_remote_.BindNewPipeAndPassReceiver());

    // Set up permission mock.
    TestBrowserContext* browser_context =
        static_cast<TestBrowserContext*>(main_rfh()->GetBrowserContext());
    browser_context->SetPermissionControllerDelegate(
        std::make_unique<TestFontAccessPermissionManager>());
    permission_controller_ =
        std::make_unique<PermissionControllerImpl>(browser_context);
  }

  void TearDown() override { RenderViewHostImplTestHarness::TearDown(); }

  TestFontAccessPermissionManager* test_permission_manager() {
    return static_cast<TestFontAccessPermissionManager*>(
        main_rfh()->GetBrowserContext()->GetPermissionControllerDelegate());
  }

  void AutoGrantPermission() {
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run(blink::mojom::PermissionStatus::GRANTED);
        }));
    test_permission_manager()->SetPermissionStatusForFrame(
        blink::mojom::PermissionStatus::GRANTED);
  }

  void AutoDenyPermission() {
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
        }));
    test_permission_manager()->SetPermissionStatusForFrame(
        blink::mojom::PermissionStatus::DENIED);
  }

  void AskGrantPermission() {
    test_permission_manager()->SetPermissionStatusForFrame(
        blink::mojom::PermissionStatus::ASK);
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run(blink::mojom::PermissionStatus::GRANTED);
        }));
  }

  void AskDenyPermission() {
    test_permission_manager()->SetPermissionStatusForFrame(
        blink::mojom::PermissionStatus::ASK);
    test_permission_manager()->SetRequestCallback(base::BindRepeating(
        [](TestFontAccessPermissionManager::PermissionCallback callback) {
          std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
        }));
  }

  void SetFrameHidden() {
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->VisibilityChanged(blink::mojom::FrameVisibility::kNotRendered);
  }

  void SimulateUserActivation() {
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->UpdateUserActivationState(
            blink::mojom::UserActivationUpdateType::kNotifyActivation,
            blink::mojom::UserActivationNotificationType::kTest);
  }

 protected:
  const GURL kTestUrl = GURL("https://example.com/font_access");
  const url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));

  std::unique_ptr<PermissionControllerImpl> permission_controller_;
  std::unique_ptr<FontAccessManagerImpl> manager_;
  mojo::Remote<blink::mojom::FontAccessManager> manager_remote_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

#if defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)
namespace {

void ValidateFontEnumerationBasic(FontEnumerationStatus status,
                                  base::ReadOnlySharedMemoryRegion region) {
  ASSERT_EQ(status, FontEnumerationStatus::kOk) << "enumeration status is kOk";

  blink::FontEnumerationTable table;
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  table.ParseFromArray(mapping.memory(), mapping.size());

  blink::FontEnumerationTable_FontMetadata previous_font;
  for (const auto& font : table.fonts()) {
    EXPECT_GT(font.postscript_name().size(), 0ULL)
        << "postscript_name size is not zero.";
    EXPECT_GT(font.full_name().size(), 0ULL) << "full_name size is not zero.";
    EXPECT_GT(font.family().size(), 0ULL) << "family size is not zero.";
    EXPECT_GE(font.stretch(), 0.5f) << "stretch is in 0.5..2.0.";
    EXPECT_LE(font.stretch(), 2.0f) << "stretch is in 0.5..2.0.";
    EXPECT_GE(font.weight(), 1.f) << "weight is in 1..1000.";
    EXPECT_LE(font.weight(), 1000.f) << "weight is in 1..1000.";

    if (previous_font.IsInitialized()) {
      EXPECT_LT(previous_font.postscript_name(), font.postscript_name())
          << "font list is sorted";
    }

    previous_font = font;
  }
}

}  // namespace

TEST_F(FontAccessManagerImplTest, FailsIfFrameNotInViewport) {
  ASSERT_TRUE(manager_remote_.is_bound() && manager_remote_.is_connected());
  AutoGrantPermission();
  SetFrameHidden();

  base::RunLoop run_loop;
  manager_remote_->EnumerateLocalFonts(
      base::BindLambdaForTesting([&](FontEnumerationStatus status,
                                     base::ReadOnlySharedMemoryRegion region) {
        EXPECT_EQ(status, FontEnumerationStatus::kNotVisible)
            << "Frame needs to be rendered";
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(FontAccessManagerImplTest, EnumerationConsumesUserActivation) {
  ASSERT_TRUE(manager_remote_.is_bound() && manager_remote_.is_connected());
  AskGrantPermission();
  SimulateUserActivation();

  base::RunLoop run_loop;
  manager_remote_->EnumerateLocalFonts(
      base::BindLambdaForTesting([&](FontEnumerationStatus status,
                                     base::ReadOnlySharedMemoryRegion region) {
        EXPECT_EQ(status, FontEnumerationStatus::kOk)
            << "Font Enumeration was successful.";
        run_loop.Quit();
      }));
  run_loop.Run();

  AskGrantPermission();

  base::RunLoop run_loop2;
  manager_remote_->EnumerateLocalFonts(
      base::BindLambdaForTesting([&](FontEnumerationStatus status,
                                     base::ReadOnlySharedMemoryRegion region) {
        EXPECT_EQ(status, FontEnumerationStatus::kNeedsUserActivation)
            << "User Activation Required.";
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

TEST_F(FontAccessManagerImplTest, PreviouslyGrantedValidateEnumerationBasic) {
  ASSERT_TRUE(manager_remote_.is_bound() && manager_remote_.is_connected());
  AutoGrantPermission();
  SimulateUserActivation();

  base::RunLoop run_loop;
  manager_remote_->EnumerateLocalFonts(
      base::BindLambdaForTesting([&](FontEnumerationStatus status,
                                     base::ReadOnlySharedMemoryRegion region) {
        EXPECT_EQ(status, FontEnumerationStatus::kOk)
            << "Font Enumeration was successful.";
        ValidateFontEnumerationBasic(std::move(status), std::move(region));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(FontAccessManagerImplTest, UserActivationRequiredBeforeGrant) {
  ASSERT_TRUE(manager_remote_.is_bound() && manager_remote_.is_connected());
  AskGrantPermission();
  SimulateUserActivation();

  base::RunLoop run_loop;
  manager_remote_->EnumerateLocalFonts(
      base::BindLambdaForTesting([&](FontEnumerationStatus status,
                                     base::ReadOnlySharedMemoryRegion region) {
        EXPECT_EQ(status, FontEnumerationStatus::kOk)
            << "Font Enumeration was successful.";
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(FontAccessManagerImplTest, EnumerationFailsIfNoActivation) {
  ASSERT_TRUE(manager_remote_.is_bound() && manager_remote_.is_connected());
  AskGrantPermission();

  base::RunLoop run_loop;
  manager_remote_->EnumerateLocalFonts(
      base::BindLambdaForTesting([&](FontEnumerationStatus status,
                                     base::ReadOnlySharedMemoryRegion region) {
        EXPECT_EQ(status, FontEnumerationStatus::kNeedsUserActivation)
            << "User Activation is needed.";
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(FontAccessManagerImplTest, PermissionDeniedOnAskErrors) {
  ASSERT_TRUE(manager_remote_.is_bound() && manager_remote_.is_connected());
  AskDenyPermission();
  SimulateUserActivation();

  base::RunLoop run_loop;
  manager_remote_->EnumerateLocalFonts(
      base::BindLambdaForTesting([&](FontEnumerationStatus status,
                                     base::ReadOnlySharedMemoryRegion region) {
        EXPECT_EQ(status, FontEnumerationStatus::kPermissionDenied)
            << "Permission was denied.";
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(FontAccessManagerImplTest, PermissionPreviouslyDeniedErrors) {
  ASSERT_TRUE(manager_remote_.is_bound() && manager_remote_.is_connected());
  AutoDenyPermission();
  SimulateUserActivation();

  base::RunLoop run_loop;
  manager_remote_->EnumerateLocalFonts(
      base::BindLambdaForTesting([&](FontEnumerationStatus status,
                                     base::ReadOnlySharedMemoryRegion region) {
        EXPECT_EQ(status, FontEnumerationStatus::kPermissionDenied)
            << "Permission was denied.";
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(FontAccessManagerImplTest, FontAccessContextFindAllFontsTest) {
  FontAccessContext* font_access_context =
      static_cast<FontAccessContext*>(manager_.get());

  base::RunLoop run_loop;
  font_access_context->FindAllFonts(base::BindLambdaForTesting(
      [&](FontEnumerationStatus status,
          std::vector<blink::mojom::FontMetadata> fonts) {
        EXPECT_EQ(status, FontEnumerationStatus::kOk)
            << "Enumeration expected to be successful.";
        EXPECT_GT(fonts.size(), 0u)
            << "Enumeration expected to yield at least 1 font";
        run_loop.Quit();
      }));
  run_loop.Run();
}

#endif

}  // namespace content
