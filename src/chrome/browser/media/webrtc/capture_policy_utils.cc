// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/capture_policy_utils.h"

#include "base/containers/cxx20_erase_vector.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#endif

namespace capture_policy {

namespace {

struct RestrictedCapturePolicy {
  const char* pref_name;
  AllowedScreenCaptureLevel capture_level;
};

}  // namespace

bool IsOriginInList(const GURL& request_origin,
                    const base::Value* allowed_origins) {
  if (!allowed_origins || !allowed_origins->is_list())
    return false;

  // Though we are not technically a Content Setting, ContentSettingsPattern
  // aligns better than URLMatcher with the rules from:
  // https://chromeenterprise.google/policies/url-patterns/.
  for (const auto& value : allowed_origins->GetList()) {
    if (!value.is_string())
      continue;
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(value.GetString());
    if (pattern.IsValid() && pattern.Matches(request_origin))
      return true;
  }

  return false;
}

AllowedScreenCaptureLevel GetAllowedCaptureLevel(
    const GURL& request_origin,
    content::WebContents* capturer_web_contents) {
  // If we can't get the PrefService, then we won't apply any restrictions.
  Profile* profile =
      Profile::FromBrowserContext(capturer_web_contents->GetBrowserContext());
  if (!profile)
    return AllowedScreenCaptureLevel::kUnrestricted;

  const PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return AllowedScreenCaptureLevel::kUnrestricted;

  return GetAllowedCaptureLevel(request_origin, *prefs);
}

AllowedScreenCaptureLevel GetAllowedCaptureLevel(const GURL& request_origin,
                                                 const PrefService& prefs) {
  // Walk through the different "levels" of restriction in priority order. If
  // an origin is in a more restrictive list, it is more restricted.
  // Note that we only store the pref name and not the pref value here, as we
  // want to look the pref value up each time (thus meaning we could not make
  // this a static), as the value can change.
  static constexpr std::array<RestrictedCapturePolicy, 4>
      kScreenCapturePolicyLists{{{prefs::kSameOriginTabCaptureAllowedByOrigins,
                                  AllowedScreenCaptureLevel::kSameOrigin},
                                 {prefs::kTabCaptureAllowedByOrigins,
                                  AllowedScreenCaptureLevel::kTab},
                                 {prefs::kWindowCaptureAllowedByOrigins,
                                  AllowedScreenCaptureLevel::kWindow},
                                 {prefs::kScreenCaptureAllowedByOrigins,
                                  AllowedScreenCaptureLevel::kDesktop}}};

  for (const auto& policy_list : kScreenCapturePolicyLists) {
    if (IsOriginInList(request_origin, prefs.GetList(policy_list.pref_name))) {
      return policy_list.capture_level;
    }
  }

  // If we've reached this point our origin wasn't in any of the override lists.
  // That means that either everything is allowed or nothing is allowed, based
  // on what |kScreenCaptureAllowed| is set to.
  if (prefs.GetBoolean(prefs::kScreenCaptureAllowed)) {
    return AllowedScreenCaptureLevel::kUnrestricted;
  }

  return AllowedScreenCaptureLevel::kDisallowed;
}

DesktopMediaList::WebContentsFilter GetIncludableWebContentsFilter(
    const GURL& request_origin,
    AllowedScreenCaptureLevel capture_level) {
  switch (capture_level) {
    case AllowedScreenCaptureLevel::kDisallowed:
      return base::BindRepeating(
          [](content::WebContents* wc) { return false; });
    case AllowedScreenCaptureLevel::kSameOrigin:
      return base::BindRepeating(
          [](const GURL& request_origin, content::WebContents* web_contents) {
            DCHECK(web_contents);
            return url::IsSameOriginWith(
                request_origin,
                web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL());
          },
          request_origin);
    default:
      return base::BindRepeating([](content::WebContents* wc) { return true; });
  }
}

void FilterMediaList(std::vector<DesktopMediaList::Type>& media_types,
                     AllowedScreenCaptureLevel capture_level) {
  base::EraseIf(
      media_types, [capture_level](const DesktopMediaList::Type& type) {
        switch (type) {
          case DesktopMediaList::Type::kNone:
            NOTREACHED();
            return false;
          // SameOrigin is more restrictive than just Tabs, so as long as
          // at least SameOrigin is allowed, these entries should stay.
          // They should be filtered later by the caller.
          case DesktopMediaList::Type::kCurrentTab:
          case DesktopMediaList::Type::kWebContents:
            return capture_level < AllowedScreenCaptureLevel::kSameOrigin;
          case DesktopMediaList::Type::kWindow:
            return capture_level < AllowedScreenCaptureLevel::kWindow;
          case DesktopMediaList::Type::kScreen:
            return capture_level < AllowedScreenCaptureLevel::kDesktop;
        }
      });
}

#if !defined(OS_ANDROID)
class CaptureTerminatedDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  explicit CaptureTerminatedDialogDelegate(content::WebContents* web_contents)
      : TabModalConfirmDialogDelegate(web_contents) {}
  ~CaptureTerminatedDialogDelegate() override = default;
  std::u16string GetTitle() override {
    return l10n_util::GetStringUTF16(
        IDS_TAB_CAPTURE_TERMINATED_BY_POLICY_TITLE);
  }

  std::u16string GetDialogMessage() override {
    return l10n_util::GetStringUTF16(IDS_TAB_CAPTURE_TERMINATED_BY_POLICY_TEXT);
  }

  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_OK; }
};
#endif

void ShowCaptureTerminatedDialog(content::WebContents* contents) {
#if !defined(OS_ANDROID)
  TabModalConfirmDialog::Create(
      std::make_unique<CaptureTerminatedDialogDelegate>(contents), contents);
#endif
}

}  // namespace capture_policy
