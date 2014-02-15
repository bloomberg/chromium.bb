// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/test/app_list_test_view_delegate.h"

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/signin_delegate.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {
namespace test {

class TestSigninDelegate : public SigninDelegate {
 public:
  TestSigninDelegate() : signed_in_(true) {}

  void set_signed_in(bool signed_in) { signed_in_ = signed_in; }

  // SigninDelegate overrides:
  virtual bool NeedSignin() OVERRIDE { return !signed_in_; }
  virtual void ShowSignin() OVERRIDE {}
  virtual void OpenLearnMore() OVERRIDE {}
  virtual void OpenSettings() OVERRIDE {}

  virtual base::string16 GetSigninHeading() OVERRIDE {
    return base::string16();
  }
  virtual base::string16 GetSigninText() OVERRIDE { return base::string16(); }
  virtual base::string16 GetSigninButtonText() OVERRIDE {
    return base::string16();
  }
  virtual base::string16 GetLearnMoreLinkText() OVERRIDE {
    return base::string16();
  }
  virtual base::string16 GetSettingsLinkText() OVERRIDE {
    return base::string16();
  }

 private:
  bool signed_in_;

  DISALLOW_COPY_AND_ASSIGN(TestSigninDelegate);
};

AppListTestViewDelegate::AppListTestViewDelegate()
    : dismiss_count_(0),
      open_search_result_count_(0),
      test_signin_delegate_(new TestSigninDelegate),
      model_(new AppListTestModel),
      speech_ui_(SPEECH_RECOGNITION_OFF) {
}

AppListTestViewDelegate::~AppListTestViewDelegate() {}

void AppListTestViewDelegate::SetSignedIn(bool signed_in) {
  test_signin_delegate_->set_signed_in(signed_in);
  FOR_EACH_OBSERVER(AppListViewDelegateObserver,
                    observers_,
                    OnProfilesChanged());
}

bool AppListTestViewDelegate::ForceNativeDesktop() const {
  return false;
}

AppListModel* AppListTestViewDelegate::GetModel() {
  return model_.get();
}

SigninDelegate* AppListTestViewDelegate::GetSigninDelegate() {
  return test_signin_delegate_.get();
}

SpeechUIModel* AppListTestViewDelegate::GetSpeechUI() {
  return &speech_ui_;
}

void AppListTestViewDelegate::GetShortcutPathForApp(
    const std::string& app_id,
    const base::Callback<void(const base::FilePath&)>& callback) {
  callback.Run(base::FilePath());
}

void AppListTestViewDelegate::OpenSearchResult(SearchResult* result,
                                               bool auto_launch,
                                               int event_flags) {
  ++open_search_result_count_;
}

base::TimeDelta AppListTestViewDelegate::GetAutoLaunchTimeout() {
  return auto_launch_timeout_;
}

void AppListTestViewDelegate::AutoLaunchCanceled() {
  auto_launch_timeout_ = base::TimeDelta();
}

void AppListTestViewDelegate::Dismiss() {
  ++dismiss_count_;
}

gfx::ImageSkia AppListTestViewDelegate::GetWindowIcon() {
  return gfx::ImageSkia();
}

content::WebContents* AppListTestViewDelegate::GetStartPageContents() {
  return NULL;
}

content::WebContents* AppListTestViewDelegate::GetSpeechRecognitionContents() {
  return NULL;
}

const AppListViewDelegate::Users& AppListTestViewDelegate::GetUsers() const {
  return users_;
}

void AppListTestViewDelegate::ReplaceTestModel(int item_count) {
  model_.reset(new AppListTestModel);
  model_->PopulateApps(item_count);
}

void AppListTestViewDelegate::AddObserver(
    AppListViewDelegateObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListTestViewDelegate::RemoveObserver(
    AppListViewDelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace test
}  // namespace app_list
