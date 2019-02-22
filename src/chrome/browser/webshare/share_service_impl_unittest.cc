// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/browser/webshare/webshare_target.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kTitle[] = "My title";
constexpr char kText[] = "My text";
constexpr char kUrlSpec[] = "https://www.google.com/";

constexpr char kTargetName[] = "Share Target";
constexpr char kParamTitle[] = "my title";
constexpr char kParamText[] = "text%";
constexpr char kParamUrl[] = "url";
constexpr char kActionHigh[] = "https://www.example-high.com/target/share";
constexpr char kActionLow[] = "https://www.example-low.com/target/share";
constexpr char kActionLowWithQuery[] =
    "https://www.example-low.com/target/share?a=b&c=d";
constexpr char kActionMin[] = "https://www.example-min.com/target/share";
constexpr char kManifestUrlHigh[] =
    "https://www.example-high.com/target/manifest.json";
constexpr char kManifestUrlLow[] =
    "https://www.example-low.com/target/manifest.json";
constexpr char kManifestUrlMin[] =
    "https://www.example-min.com/target/manifest.json";

void DidShare(blink::mojom::ShareError expected_error,
              blink::mojom::ShareError error) {
  EXPECT_EQ(expected_error, error);
}

class ShareServiceTestImpl : public ShareServiceImpl {
 public:
  explicit ShareServiceTestImpl(blink::mojom::ShareServiceRequest request)
      : binding_(this) {
    binding_.Bind(std::move(request));

    pref_service_.reset(new TestingPrefServiceSimple());
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kWebShareVisitedTargets);
  }

  void AddShareTargetToPrefs(const std::string& manifest_url,
                             const std::string& name,
                             const std::string& action,
                             const std::string& text,
                             const std::string& title,
                             const std::string& url) {
    constexpr char kActionKey[] = "action";
    constexpr char kNameKey[] = "name";
    constexpr char kTextKey[] = "text";
    constexpr char kTitleKey[] = "title";
    constexpr char kUrlKey[] = "url";

    DictionaryPrefUpdate update(GetPrefService(),
                                prefs::kWebShareVisitedTargets);
    base::DictionaryValue* share_target_dict = update.Get();

    std::unique_ptr<base::DictionaryValue> origin_dict(
        new base::DictionaryValue);

    origin_dict->SetKey(kActionKey, base::Value(action));
    origin_dict->SetKey(kNameKey, base::Value(name));
    origin_dict->SetKey(kTextKey, base::Value(text));
    origin_dict->SetKey(kTitleKey, base::Value(title));
    origin_dict->SetKey(kUrlKey, base::Value(url));

    share_target_dict->SetWithoutPathExpansion(manifest_url,
                                               std::move(origin_dict));
  }

  void SetEngagementForTarget(const std::string& manifest_url,
                              blink::mojom::EngagementLevel level) {
    engagement_map_[manifest_url] = level;
  }

  void set_run_loop(base::RunLoop* run_loop) {
    quit_run_loop_ = run_loop->QuitClosure();
  }

  const std::string& GetLastUsedTargetURL() { return last_used_target_url_; }

  const std::vector<WebShareTarget>& GetTargetsInPicker() {
    return targets_in_picker_;
  }

  void PickTarget(const std::string& target_url) {
    const auto& it =
        std::find_if(targets_in_picker_.begin(), targets_in_picker_.end(),
                     [&target_url](const WebShareTarget& target) {
                       return target.manifest_url().spec() == target_url;
                     });
    DCHECK(it != targets_in_picker_.end());
    std::move(picker_callback_).Run(&*it);
  }

  chrome::WebShareTargetPickerCallback picker_callback() {
    return std::move(picker_callback_);
  }

 private:
  void ShowPickerDialog(
      std::vector<WebShareTarget> targets,
      chrome::WebShareTargetPickerCallback callback) override {
    // Store the arguments passed to the picker dialog.
    targets_in_picker_ = std::move(targets);
    picker_callback_ = std::move(callback);

    // Quit the test's run loop. It is the test's responsibility to call the
    // callback, to simulate the user's choice.
    std::move(quit_run_loop_).Run();
  }

  void OpenTargetURL(const GURL& target_url) override {
    last_used_target_url_ = target_url.spec();
  }

  PrefService* GetPrefService() override { return pref_service_.get(); }

  blink::mojom::EngagementLevel GetEngagementLevel(const GURL& url) override {
    return engagement_map_[url.spec()];
  }

  mojo::Binding<blink::mojom::ShareService> binding_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  std::map<std::string, blink::mojom::EngagementLevel> engagement_map_;
  // Closure to quit the test's run loop.
  base::OnceClosure quit_run_loop_;

  // The last URL passed to OpenTargetURL.
  std::string last_used_target_url_;
  // The targets passed to ShowPickerDialog.
  std::vector<WebShareTarget> targets_in_picker_;
  // The callback passed to ShowPickerDialog (which is supposed to be called
  // with the user's chosen result, or nullptr if cancelled).
  chrome::WebShareTargetPickerCallback picker_callback_;
};

class ShareServiceImplUnittest : public ChromeRenderViewHostTestHarness {
 public:
  ShareServiceImplUnittest() = default;
  ~ShareServiceImplUnittest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    share_service_helper_ = std::make_unique<ShareServiceTestImpl>(
        mojo::MakeRequest(&share_service_));

    share_service_helper_->SetEngagementForTarget(
        kManifestUrlHigh, blink::mojom::EngagementLevel::HIGH);
    share_service_helper_->SetEngagementForTarget(
        kManifestUrlMin, blink::mojom::EngagementLevel::MINIMAL);
    share_service_helper_->SetEngagementForTarget(
        kManifestUrlLow, blink::mojom::EngagementLevel::LOW);
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  blink::mojom::ShareService* share_service() const {
    return share_service_.get();
  }

  ShareServiceTestImpl* share_service_helper() const {
    return share_service_helper_.get();
  }

  void DeleteShareService() { share_service_helper_.reset(); }

 private:
  blink::mojom::ShareServicePtr share_service_;
  std::unique_ptr<ShareServiceTestImpl> share_service_helper_;
};

}  // namespace

// Basic test to check the Share method calls the callback with the expected
// parameters.
TEST_F(ShareServiceImplUnittest, ShareCallbackParams) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kActionLow, kParamText,
                                                kParamTitle, kParamUrl);
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlHigh, kTargetName,
                                                kActionHigh, kParamText,
                                                kParamTitle, kParamUrl);
  // Expect this invalid URL to be ignored (not crash);
  // https://crbug.com/762388.
  share_service_helper()->AddShareTargetToPrefs(
      "", kTargetName, kActionHigh, kParamText, kParamTitle, kParamUrl);

  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::OK);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlHigh), kTargetName,
                                GURL(kActionHigh), kParamText, kParamTitle,
                                kParamUrl);
  expected_targets.emplace_back(GURL(kManifestUrlLow), kTargetName,
                                GURL(kActionLow), kParamText, kParamTitle,
                                kParamUrl);
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  // Pick example-low.com.
  share_service_helper()->PickTarget(kManifestUrlLow);

  const char kExpectedURL[] =
      "https://www.example-low.com/target/"
      "share?my+title=My+title&text%25=My+text&url=https%3A%2F%2Fwww."
      "google.com%2F";
  EXPECT_EQ(kExpectedURL, share_service_helper()->GetLastUsedTargetURL());
}

// Adds URL already containing query parameters.
TEST_F(ShareServiceImplUnittest, ShareCallbackWithQueryString) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kActionLowWithQuery, kParamText,
                                                kParamTitle, kParamUrl);
  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::OK);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlLow), kTargetName,
                                GURL(kActionLowWithQuery), kParamText,
                                kParamTitle, kParamUrl);
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  // Pick example-low.com.
  share_service_helper()->PickTarget(kManifestUrlLow);

  const char kExpectedURL[] =
      "https://www.example-low.com/target/"
      "share?my+title=My+title&text%25=My+text&url=https%3A%2F%2Fwww."
      "google.com%2F";
  EXPECT_EQ(kExpectedURL, share_service_helper()->GetLastUsedTargetURL());
}

// Tests the result of cancelling the share in the picker UI, that doesn't have
// any targets.
TEST_F(ShareServiceImplUnittest, ShareCancelNoTargets) {
  // Expect an error message in response.
  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::CANCELED);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  EXPECT_TRUE(share_service_helper()->GetTargetsInPicker().empty());

  // Cancel the dialog.
  share_service_helper()->picker_callback().Run(nullptr);

  EXPECT_TRUE(share_service_helper()->GetLastUsedTargetURL().empty());
}

// Tests the result of cancelling the share in the picker UI, that has targets.
TEST_F(ShareServiceImplUnittest, ShareCancelWithTargets) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlHigh, kTargetName,
                                                kActionHigh, kParamText,
                                                kParamTitle, kParamUrl);
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kActionLow, kParamText,
                                                kParamTitle, kParamUrl);

  // Expect an error message in response.
  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::CANCELED);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlHigh), kTargetName,
                                GURL(kActionHigh), kParamText, kParamTitle,
                                kParamUrl);
  expected_targets.emplace_back(GURL(kManifestUrlLow), kTargetName,
                                GURL(kActionLow), kParamText, kParamTitle,
                                kParamUrl);
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  // Cancel the dialog.
  share_service_helper()->picker_callback().Run(nullptr);

  EXPECT_TRUE(share_service_helper()->GetLastUsedTargetURL().empty());
}

// Test to check that only targets with enough engagement were in picker.
TEST_F(ShareServiceImplUnittest, ShareWithSomeInsufficientlyEngagedTargets) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlMin, kTargetName,
                                                kActionMin, kParamText,
                                                kParamTitle, kParamUrl);
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kActionLow, kParamText,
                                                kParamTitle, kParamUrl);

  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce(&DidShare, blink::mojom::ShareError::OK);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlLow), kTargetName,
                                GURL(kActionLow), kParamText, kParamTitle,
                                kParamUrl);
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  // Pick example-low.com.
  share_service_helper()->PickTarget(kManifestUrlLow);

  const char kExpectedURL[] =
      "https://www.example-low.com/target/"
      "share?my+title=My+title&text%25=My+text&url=https%3A%2F%2Fwww."
      "google.com%2F";
  EXPECT_EQ(kExpectedURL, share_service_helper()->GetLastUsedTargetURL());
}

// Test that deleting the share service while the picker is open does not crash
// (https://crbug.com/690775).
TEST_F(ShareServiceImplUnittest, ShareServiceDeletion) {
  share_service_helper()->AddShareTargetToPrefs(kManifestUrlLow, kTargetName,
                                                kActionLow, kParamText,
                                                kParamTitle, kParamUrl);

  base::RunLoop run_loop;
  share_service_helper()->set_run_loop(&run_loop);

  const GURL url(kUrlSpec);
  // Expect the callback to never be called (since the share service is
  // destroyed before the picker is closed).
  // TODO(mgiuca): This probably should still complete the share, if not
  // cancelled, even if the underlying tab is closed.
  base::OnceCallback<void(blink::mojom::ShareError)> callback =
      base::BindOnce([](blink::mojom::ShareError error) { FAIL(); });
  share_service()->Share(kTitle, kText, url, std::move(callback));

  run_loop.Run();

  std::vector<WebShareTarget> expected_targets;
  expected_targets.emplace_back(GURL(kManifestUrlLow), kTargetName,
                                GURL(kActionLow), kParamText, kParamTitle,
                                kParamUrl);
  EXPECT_EQ(expected_targets, share_service_helper()->GetTargetsInPicker());

  chrome::WebShareTargetPickerCallback picker_callback =
      share_service_helper()->picker_callback();

  DeleteShareService();

  // Pick example-low.com.
  std::move(picker_callback).Run(&expected_targets[0]);
}
