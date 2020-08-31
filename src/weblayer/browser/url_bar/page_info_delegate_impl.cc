// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/url_bar/page_info_delegate_impl.h"

#include "build/build_config.h"
#include "components/permissions/permission_manager.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_state/content/content_utils.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "weblayer/browser/permissions/permission_manager_factory.h"
#include "weblayer/browser/stateful_ssl_host_state_delegate_factory.h"
#include "weblayer/browser/tab_specific_content_settings_delegate.h"

#if defined(OS_ANDROID)
#include "weblayer/browser/weblayer_impl_android.h"
#endif

namespace weblayer {

PageInfoDelegateImpl::PageInfoDelegateImpl(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents_);
}

permissions::ChooserContextBase* PageInfoDelegateImpl::GetChooserContext(
    ContentSettingsType type) {
  // TODO(crbug.com/1052375): Once WebLayer has USB and Bluetooth support,
  // add more logic here.
  return nullptr;
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
safe_browsing::PasswordProtectionService*
PageInfoDelegateImpl::GetPasswordProtectionService() const {
  NOTREACHED();
  return nullptr;
}

void PageInfoDelegateImpl::OnUserActionOnPasswordUi(
    content::WebContents* web_contents,
    safe_browsing::WarningAction action) {
  NOTREACHED();
}

base::string16 PageInfoDelegateImpl::GetWarningDetailText() {
  // TODO(crbug.com/1052375): Implement.
  NOTREACHED();
  return base::string16();
}
#endif

permissions::PermissionResult PageInfoDelegateImpl::GetPermissionStatus(
    ContentSettingsType type,
    const GURL& site_url) {
  return PermissionManagerFactory::GetForBrowserContext(GetBrowserContext())
      ->GetPermissionStatus(type, site_url, site_url);
}

#if !defined(OS_ANDROID)
bool PageInfoDelegateImpl::CreateInfoBarDelegate() {
  NOTREACHED();
  return false;
}

void PageInfoDelegateImpl::ShowSiteSettings(const GURL& site_url) {
  // TODO(crbug.com/1052375): Implement once site settings code has been
  // componentized.
  NOTREACHED();
}
#endif

permissions::PermissionDecisionAutoBlocker*
PageInfoDelegateImpl::GetPermissionDecisionAutoblocker() {
  return PermissionDecisionAutoBlockerFactory::GetForBrowserContext(
      GetBrowserContext());
}

StatefulSSLHostStateDelegate*
PageInfoDelegateImpl::GetStatefulSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetInstance()
      ->GetForBrowserContext(GetBrowserContext());
}

HostContentSettingsMap* PageInfoDelegateImpl::GetContentSettings() {
  return HostContentSettingsMapFactory::GetForBrowserContext(
      GetBrowserContext());
}

bool PageInfoDelegateImpl::IsContentDisplayedInVrHeadset() {
  // VR is not supported for WebLayer.
  return false;
}

security_state::SecurityLevel PageInfoDelegateImpl::GetSecurityLevel() {
  auto state = security_state::GetVisibleSecurityState(web_contents_);
  DCHECK(state);
  return security_state::GetSecurityLevel(
      *state,
      /* used_policy_installed_certificate */ false);
}

security_state::VisibleSecurityState
PageInfoDelegateImpl::GetVisibleSecurityState() {
  return *security_state::GetVisibleSecurityState(web_contents_);
}

std::unique_ptr<content_settings::TabSpecificContentSettings::Delegate>
PageInfoDelegateImpl::GetTabSpecificContentSettingsDelegate() {
  return std::make_unique<TabSpecificContentSettingsDelegate>(web_contents_);
}

#if defined(OS_ANDROID)
const base::string16 PageInfoDelegateImpl::GetClientApplicationName() {
  return weblayer::GetClientApplicationName();
}
#endif

content::BrowserContext* PageInfoDelegateImpl::GetBrowserContext() const {
  return web_contents_->GetBrowserContext();
}

}  //  namespace weblayer
