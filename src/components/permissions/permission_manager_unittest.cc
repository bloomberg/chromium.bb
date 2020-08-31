// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif  // defined(OS_ANDROID)

using blink::mojom::PermissionStatus;
using content::PermissionType;

namespace permissions {
namespace {

class FakePermissionContext : public PermissionContextBase {
 public:
  FakePermissionContext(
      content::BrowserContext* browser_context,
      ContentSettingsType content_settings_type,
      blink::mojom::FeaturePolicyFeature feature_policy_feature)
      : PermissionContextBase(browser_context,
                              content_settings_type,
                              feature_policy_feature) {}

  // PermissionContextBase:
  bool IsRestrictedToSecureOrigins() const override { return true; }
};

class FakePermissionContextAlwaysAllow : public FakePermissionContext {
 public:
  FakePermissionContextAlwaysAllow(
      content::BrowserContext* browser_context,
      ContentSettingsType content_settings_type,
      blink::mojom::FeaturePolicyFeature feature_policy_feature)
      : FakePermissionContext(browser_context,
                              content_settings_type,
                              feature_policy_feature) {}

  // PermissionContextBase:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override {
    return CONTENT_SETTING_ALLOW;
  }
};

PermissionManager::PermissionContextMap CreatePermissionContexts(
    content::BrowserContext* browser_context) {
  PermissionManager::PermissionContextMap permission_contexts;
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::GEOLOCATION,
          blink::mojom::FeaturePolicyFeature::kGeolocation);
  permission_contexts[ContentSettingsType::NOTIFICATIONS] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::NOTIFICATIONS,
          blink::mojom::FeaturePolicyFeature::kNotFound);
  permission_contexts[ContentSettingsType::MIDI_SYSEX] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::MIDI_SYSEX,
          blink::mojom::FeaturePolicyFeature::kMidiFeature);
  permission_contexts[ContentSettingsType::MIDI] =
      std::make_unique<FakePermissionContextAlwaysAllow>(
          browser_context, ContentSettingsType::MIDI,
          blink::mojom::FeaturePolicyFeature::kMidiFeature);
#if defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER] =
      std::make_unique<FakePermissionContext>(
          browser_context, ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
          blink::mojom::FeaturePolicyFeature::kEncryptedMedia);
#endif
  return permission_contexts;
}

#if defined(OS_ANDROID)
// See https://crbug.com/904883.
auto GetDefaultProtectedMediaIdentifierPermissionStatus() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
                 base::android::SDK_VERSION_MARSHMALLOW
             ? PermissionStatus::GRANTED
             : PermissionStatus::ASK;
}

auto GetDefaultProtectedMediaIdentifierContentSetting() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
                 base::android::SDK_VERSION_MARSHMALLOW
             ? CONTENT_SETTING_ALLOW
             : CONTENT_SETTING_ASK;
}
#endif  // defined(OS_ANDROID)

}  // namespace

class PermissionManagerTest : public content::RenderViewHostTestHarness {
 public:
  void OnPermissionChange(PermissionStatus permission) {
    if (!quit_closure_.is_null())
      quit_closure_.Run();
    callback_called_ = true;
    callback_result_ = permission;
  }

 protected:
  PermissionManagerTest()
      : url_("https://example.com"),
        other_url_("https://foo.com"),
        callback_called_(false),
        callback_result_(PermissionStatus::ASK) {}

  PermissionManager* GetPermissionControllerDelegate() {
    return static_cast<PermissionManager*>(
        browser_context_->GetPermissionControllerDelegate());
  }

  HostContentSettingsMap* GetHostContentSettingsMap() {
    return PermissionsClient::Get()->GetSettingsMap(browser_context_.get());
  }

  void CheckPermissionStatus(PermissionType type, PermissionStatus expected) {
    EXPECT_EQ(expected, GetPermissionControllerDelegate()->GetPermissionStatus(
                            type, url_.GetOrigin(), url_.GetOrigin()));
  }

  void CheckPermissionResult(ContentSettingsType type,
                             ContentSetting expected_status,
                             PermissionStatusSource expected_status_source) {
    PermissionResult result =
        GetPermissionControllerDelegate()->GetPermissionStatus(
            type, url_.GetOrigin(), url_.GetOrigin());
    EXPECT_EQ(expected_status, result.content_setting);
    EXPECT_EQ(expected_status_source, result.source);
  }

  void SetPermission(ContentSettingsType type, ContentSetting value) {
    GetHostContentSettingsMap()->SetContentSettingDefaultScope(
        url_, url_, type, std::string(), value);
  }

  int RequestPermission(PermissionType type,
                        content::RenderFrameHost* rfh,
                        const GURL& origin) {
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    int result = GetPermissionControllerDelegate()->RequestPermission(
        type, rfh, origin, true,
        base::Bind(&PermissionManagerTest::OnPermissionChange,
                   base::Unretained(this)));
    loop.Run();
    return result;
  }

  const GURL& url() const { return url_; }

  const GURL& other_url() const { return other_url_; }

  bool callback_called() const { return callback_called_; }

  PermissionStatus callback_result() const { return callback_result_; }

  void Reset() {
    callback_called_ = false;
    callback_result_ = PermissionStatus::ASK;
  }

  bool PendingRequestsEmpty() {
    return GetPermissionControllerDelegate()->pending_requests_.IsEmpty();
  }

  // The header policy should only be set once on page load, so we refresh the
  // page to simulate that.
  void RefreshPageAndSetHeaderPolicy(content::RenderFrameHost** rfh,
                                     blink::mojom::FeaturePolicyFeature feature,
                                     const std::vector<std::string>& origins) {
    content::RenderFrameHost* current = *rfh;
    SimulateNavigation(&current, current->GetLastCommittedURL());
    std::vector<url::Origin> parsed_origins;
    for (const std::string& origin : origins)
      parsed_origins.push_back(url::Origin::Create(GURL(origin)));
    content::RenderFrameHostTester::For(current)->SimulateFeaturePolicyHeader(
        feature, parsed_origins);
    *rfh = current;
  }

  content::RenderFrameHost* AddChildRFH(content::RenderFrameHost* parent,
                                        const char* origin) {
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChild("");
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, GURL(origin));
    return result;
  }

 private:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    browser_context_->SetPermissionControllerDelegate(
        std::make_unique<PermissionManager>(
            browser_context_.get(),
            CreatePermissionContexts(browser_context_.get())));
    NavigateAndCommit(url());
  }

  void TearDown() override {
    GetPermissionControllerDelegate()->Shutdown();
    browser_context_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

  const GURL url_;
  const GURL other_url_;
  bool callback_called_;
  PermissionStatus callback_result_;
  base::Closure quit_closure_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  TestPermissionsClient client_;
};

TEST_F(PermissionManagerTest, GetPermissionStatusDefault) {
  CheckPermissionStatus(PermissionType::MIDI_SYSEX, PermissionStatus::ASK);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS, PermissionStatus::ASK);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::ASK);
#if defined(OS_ANDROID)
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        GetDefaultProtectedMediaIdentifierPermissionStatus());
#endif
}

TEST_F(PermissionManagerTest, GetPermissionStatusAfterSet) {
  SetPermission(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  SetPermission(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::NOTIFICATIONS,
                        PermissionStatus::GRANTED);

  SetPermission(ContentSettingsType::MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::MIDI_SYSEX, PermissionStatus::GRANTED);

#if defined(OS_ANDROID)
  SetPermission(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                        PermissionStatus::GRANTED);
#endif
}

TEST_F(PermissionManagerTest, CheckPermissionResultDefault) {
  CheckPermissionResult(ContentSettingsType::MIDI_SYSEX, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
  CheckPermissionResult(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ASK,
                        PermissionStatusSource::UNSPECIFIED);
#if defined(OS_ANDROID)
  CheckPermissionResult(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                        GetDefaultProtectedMediaIdentifierContentSetting(),
                        PermissionStatusSource::UNSPECIFIED);
#endif
}

TEST_F(PermissionManagerTest, CheckPermissionResultAfterSet) {
  SetPermission(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

  SetPermission(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(ContentSettingsType::NOTIFICATIONS,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

  SetPermission(ContentSettingsType::MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionResult(ContentSettingsType::MIDI_SYSEX, CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);

#if defined(OS_ANDROID)
  SetPermission(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionResult(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                        CONTENT_SETTING_ALLOW,
                        PermissionStatusSource::UNSPECIFIED);
#endif
}

TEST_F(PermissionManagerTest, SubscriptionDestroyedCleanlyWithoutUnsubscribe) {
  // Test that the PermissionManager shuts down cleanly with subscriptions that
  // haven't been removed, crbug.com/720071.
  GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, main_rfh(), url(),
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));
}

TEST_F(PermissionManagerTest, SubscribeUnsubscribeAfterShutdown) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  // Simulate Keyed Services shutdown pass. Note: Shutdown will be called second
  // time during browser_context destruction. This is ok for now: Shutdown is
  // reenterant.
  GetPermissionControllerDelegate()->Shutdown();

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);

  // Check that subscribe/unsubscribe after shutdown don't crash.
  int subscription2_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription2_id);
}

TEST_F(PermissionManagerTest, SameTypeChangeNotifies) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, DifferentTypeChangeDoesNotNotify) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), GURL(), ContentSettingsType::NOTIFICATIONS, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ChangeAfterUnsubscribeDoesNotNotify) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());
}

TEST_F(PermissionManagerTest, DifferentPrimaryUrlDoesNotNotify) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      other_url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, DifferentSecondaryUrlDoesNotNotify) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), other_url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, WildCardPatternNotifies) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ClearSettingsNotifies) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->ClearSettingsForOneType(
      ContentSettingsType::GEOLOCATION);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, NewValueCorrectlyPassed) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ChangeWithoutPermissionChangeDoesNotNotify) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ChangesBackAndForth) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  Reset();

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, ChangesBackAndForthWorker) {
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, nullptr, url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  Reset();

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, SubscribeMIDIPermission) {
  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::MIDI, main_rfh(), url(),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));

  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::ASK);
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

TEST_F(PermissionManagerTest, PermissionIgnoredCleanup) {
  content::WebContents* contents = web_contents();
  PermissionRequestManager::CreateForWebContents(contents);
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(contents);
  auto prompt_factory = std::make_unique<MockPermissionPromptFactory>(manager);

  NavigateAndCommit(url());

  GetPermissionControllerDelegate()->RequestPermission(
      PermissionType::GEOLOCATION, main_rfh(), url(), /*user_gesture=*/true,
      base::Bind(&PermissionManagerTest::OnPermissionChange,
                 base::Unretained(this)));

  EXPECT_FALSE(PendingRequestsEmpty());

  NavigateAndCommit(GURL("https://foobar.com"));

  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(PendingRequestsEmpty());
}

// Check PermissionResult shows requests denied due to insecure
// origins.
TEST_F(PermissionManagerTest, InsecureOrigin) {
  GURL insecure_frame("http://www.example.com/geolocation");
  NavigateAndCommit(insecure_frame);

  PermissionResult result =
      GetPermissionControllerDelegate()->GetPermissionStatusForFrame(
          ContentSettingsType::GEOLOCATION, web_contents()->GetMainFrame(),
          insecure_frame);

  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::INSECURE_ORIGIN, result.source);

  GURL secure_frame("https://www.example.com/geolocation");
  NavigateAndCommit(secure_frame);

  result = GetPermissionControllerDelegate()->GetPermissionStatusForFrame(
      ContentSettingsType::GEOLOCATION, web_contents()->GetMainFrame(),
      secure_frame);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
}

TEST_F(PermissionManagerTest, InsecureOriginIsNotOverridable) {
  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://example.com/geolocation"));
  const url::Origin kSecureOrigin =
      url::Origin::Create(GURL("https://example.com/geolocation"));
  EXPECT_FALSE(
      GetPermissionControllerDelegate()->IsPermissionOverridableByDevTools(
          PermissionType::GEOLOCATION, kInsecureOrigin));
  EXPECT_TRUE(
      GetPermissionControllerDelegate()->IsPermissionOverridableByDevTools(
          PermissionType::GEOLOCATION, kSecureOrigin));
}

TEST_F(PermissionManagerTest, MissingContextIsNotOverridable) {
  // Permissions that are not implemented should be denied overridability.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  EXPECT_FALSE(
      GetPermissionControllerDelegate()->IsPermissionOverridableByDevTools(
          PermissionType::PROTECTED_MEDIA_IDENTIFIER,
          url::Origin::Create(GURL("http://localhost"))));
#endif
  EXPECT_TRUE(
      GetPermissionControllerDelegate()->IsPermissionOverridableByDevTools(
          PermissionType::MIDI_SYSEX,
          url::Origin::Create(GURL("http://localhost"))));
}

TEST_F(PermissionManagerTest, KillSwitchOnIsNotOverridable) {
  const url::Origin kLocalHost = url::Origin::Create(GURL("http://localhost"));
  EXPECT_TRUE(
      GetPermissionControllerDelegate()->IsPermissionOverridableByDevTools(
          PermissionType::GEOLOCATION, kLocalHost));

  // Turn on kill switch for GEOLOCATION.
  std::map<std::string, std::string> params;
  params[PermissionUtil::GetPermissionString(
      ContentSettingsType::GEOLOCATION)] =
      PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  base::AssociateFieldTrialParams(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy, "TestGroup",
      params);
  base::FieldTrialList::CreateFieldTrial(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy, "TestGroup");

  EXPECT_FALSE(
      GetPermissionControllerDelegate()->IsPermissionOverridableByDevTools(
          PermissionType::GEOLOCATION, kLocalHost));
}

TEST_F(PermissionManagerTest, GetPermissionStatusDelegation) {
  const char* kOrigin1 = "https://example.com";
  const char* kOrigin2 = "https://google.com";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPermissionDelegation);

  NavigateAndCommit(GURL(kOrigin1));
  content::RenderFrameHost* parent = main_rfh();

  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  // By default the parent should be able to request access, but not the child.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              parent, GURL(kOrigin1))
                .content_setting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // Enabling geolocation by FP should allow the child to request access also.
  RefreshPageAndSetHeaderPolicy(
      &parent, blink::mojom::FeaturePolicyFeature::kGeolocation,
      {kOrigin1, kOrigin2});
  child = AddChildRFH(parent, kOrigin2);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // When the child requests location a prompt should be displayed for the
  // parent.
  PermissionRequestManager::CreateForWebContents(web_contents());
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  auto prompt_factory = std::make_unique<MockPermissionPromptFactory>(manager);
  prompt_factory->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  prompt_factory->DocumentOnLoadCompletedInMainFrame();

  RequestPermission(PermissionType::GEOLOCATION, child, GURL(kOrigin2));

  EXPECT_TRUE(prompt_factory->RequestOriginSeen(GURL(kOrigin1)));

  // Now the child frame should have location, as well as the parent frame.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              parent, GURL(kOrigin1))
                .content_setting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // Revoking access from the parent should cause the child not to have access
  // either.
  GetPermissionControllerDelegate()->ResetPermission(
      PermissionType::GEOLOCATION, GURL(kOrigin1), GURL(kOrigin1));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              parent, GURL(kOrigin1))
                .content_setting);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // If the parent changes its policy, the child should be blocked.
  RefreshPageAndSetHeaderPolicy(
      &parent, blink::mojom::FeaturePolicyFeature::kGeolocation, {kOrigin1});
  child = AddChildRFH(parent, kOrigin2);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              parent, GURL(kOrigin1))
                .content_setting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  prompt_factory.reset();
}

TEST_F(PermissionManagerTest, SubscribeWithPermissionDelegation) {
  const char* kOrigin1 = "https://example.com";
  const char* kOrigin2 = "https://google.com";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPermissionDelegation);

  NavigateAndCommit(GURL(kOrigin1));
  content::RenderFrameHost* parent = main_rfh();
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  int subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, child, GURL(kOrigin2),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));
  EXPECT_FALSE(callback_called());

  // Location should be blocked for the child because it's not delegated.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  // Allow access for the top level origin.
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);

  // The child's permission should still be block and no callback should be run.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  EXPECT_FALSE(callback_called());

  // Enabling geolocation by FP should allow the child to request access also.
  RefreshPageAndSetHeaderPolicy(
      &parent, blink::mojom::FeaturePolicyFeature::kGeolocation,
      {kOrigin1, kOrigin2});
  child = AddChildRFH(parent, kOrigin2);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  subscription_id =
      GetPermissionControllerDelegate()->SubscribePermissionStatusChange(
          PermissionType::GEOLOCATION, child, GURL(kOrigin2),
          base::Bind(&PermissionManagerTest::OnPermissionChange,
                     base::Unretained(this)));
  EXPECT_FALSE(callback_called());

  // Blocking access to the parent should trigger the callback to be run for the
  // child also.
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url(), url(), ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionControllerDelegate()
                ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION,
                                              child, GURL(kOrigin2))
                .content_setting);

  GetPermissionControllerDelegate()->UnsubscribePermissionStatusChange(
      subscription_id);
}

}  // namespace permissions
