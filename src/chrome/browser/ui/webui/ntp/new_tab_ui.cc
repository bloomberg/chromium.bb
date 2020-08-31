// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/ui/webui/theme_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

namespace {

// Strings sent to the page via jstemplates used to set the direction of the
// HTML document based on locale.
const char kRTLHtmlTextDirection[] = "rtl";
const char kLTRHtmlTextDirection[] = "ltr";

const char* GetHtmlTextDirection(const base::string16& text) {
  if (base::i18n::IsRTL() && base::i18n::StringContainsStrongRTLChars(text))
    return kRTLHtmlTextDirection;
  return kLTRHtmlTextDirection;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabUI

NewTabUI::NewTabUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));

  Profile* profile = GetProfile();

  if (!profile->IsGuestSession()) {
    web_ui->AddMessageHandler(std::make_unique<ThemeHandler>());
    web_ui->AddMessageHandler(std::make_unique<CookieControlsHandler>(profile));
  }

  // content::URLDataSource assumes the ownership of the html source.
  content::URLDataSource::Add(profile, std::make_unique<NewTabHTMLSource>(
                                           profile->GetOriginalProfile()));

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(bookmarks::prefs::kShowBookmarkBar,
                             base::Bind(&NewTabUI::OnShowBookmarkBarChanged,
                                        base::Unretained(this)));
}

NewTabUI::~NewTabUI() {}

void NewTabUI::OnShowBookmarkBarChanged() {
  base::Value attached(
      GetProfile()->GetPrefs()->GetBoolean(bookmarks::prefs::kShowBookmarkBar)
          ? "true"
          : "false");
  web_ui()->CallJavascriptFunctionUnsafe("ntp.setBookmarkBarAttached",
                                         attached);
}

// static
void NewTabUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  CoreAppLauncherHandler::RegisterProfilePrefs(registry);
  AppLauncherHandler::RegisterProfilePrefs(registry);
}

// static
bool NewTabUI::IsNewTab(const GURL& url) {
  return url.GetOrigin() == GURL(chrome::kChromeUINewTabURL).GetOrigin();
}

// static
void NewTabUI::SetUrlTitleAndDirection(base::Value* dictionary,
                                       const base::string16& title,
                                       const GURL& gurl) {
  dictionary->SetStringKey("url", gurl.spec());

  bool using_url_as_the_title = false;
  base::string16 title_to_set(title);
  if (title_to_set.empty()) {
    using_url_as_the_title = true;
    title_to_set = base::UTF8ToUTF16(gurl.spec());
  }

  // We set the "dir" attribute of the title, so that in RTL locales, a LTR
  // title is rendered left-to-right and truncated from the right. For example,
  // the title of http://msdn.microsoft.com/en-us/default.aspx is "MSDN:
  // Microsoft developer network". In RTL locales, in the [New Tab] page, if
  // the "dir" of this title is not specified, it takes Chrome UI's
  // directionality. So the title will be truncated as "soft developer
  // network". Setting the "dir" attribute as "ltr" renders the truncated title
  // as "MSDN: Microsoft D...". As another example, the title of
  // http://yahoo.com is "Yahoo!". In RTL locales, in the [New Tab] page, the
  // title will be rendered as "!Yahoo" if its "dir" attribute is not set to
  // "ltr".
  std::string direction;
  if (using_url_as_the_title)
    direction = kLTRHtmlTextDirection;
  else
    direction = GetHtmlTextDirection(title);

  dictionary->SetStringKey("title", title_to_set);
  dictionary->SetStringKey("direction", direction);
}

// static
void NewTabUI::SetFullNameAndDirection(const base::string16& full_name,
                                       base::DictionaryValue* dictionary) {
  dictionary->SetString("full_name", full_name);
  dictionary->SetString("full_name_direction", GetHtmlTextDirection(full_name));
}

Profile* NewTabUI::GetProfile() const {
  return Profile::FromWebUI(web_ui());
}

///////////////////////////////////////////////////////////////////////////////
// NewTabHTMLSource

NewTabUI::NewTabHTMLSource::NewTabHTMLSource(Profile* profile)
    : profile_(profile) {
}

std::string NewTabUI::NewTabHTMLSource::GetSource() {
  return chrome::kChromeUINewTabHost;
}

void NewTabUI::NewTabHTMLSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug/1009127): Simplify usages of |path| since |url| is available.
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  if (!path.empty() && path[0] != '#') {
    // A path under new-tab was requested; it's likely a bad relative
    // URL from the new tab page, but in any case it's an error.
    NOTREACHED() << path << " should not have been requested on the NTP";
    std::move(callback).Run(nullptr);
    return;
  }

  content::WebContents* web_contents = wc_getter.Run();
  content::RenderProcessHost* render_host =
      web_contents ? web_contents->GetMainFrame()->GetProcess() : nullptr;
  NTPResourceCache::WindowType win_type = NTPResourceCache::GetWindowType(
      profile_, render_host);
  scoped_refptr<base::RefCountedMemory> html_bytes(
      NTPResourceCacheFactory::GetForProfile(profile_)->
      GetNewTabHTML(win_type));

  std::move(callback).Run(html_bytes.get());
}

std::string NewTabUI::NewTabHTMLSource::GetMimeType(
    const std::string& resource) {
  return "text/html";
}

bool NewTabUI::NewTabHTMLSource::ShouldReplaceExistingSource() {
  return false;
}

std::string NewTabUI::NewTabHTMLSource::GetContentSecurityPolicyScriptSrc() {
  // 'unsafe-inline' and google resources are added to script-src.
  return "script-src chrome://resources 'self' 'unsafe-eval' 'unsafe-inline' "
      "*.google.com *.gstatic.com;";
}

std::string NewTabUI::NewTabHTMLSource::GetContentSecurityPolicyStyleSrc() {
  return "style-src 'self' chrome://resources 'unsafe-inline' chrome://theme;";
}

std::string NewTabUI::NewTabHTMLSource::GetContentSecurityPolicyImgSrc() {
  return "img-src chrome-search://thumb chrome-search://thumb2 "
      "chrome-search://theme chrome://theme data:;";
}

std::string NewTabUI::NewTabHTMLSource::GetContentSecurityPolicyChildSrc() {
  return "child-src chrome-search://most-visited;";
}

NewTabUI::NewTabHTMLSource::~NewTabHTMLSource() {}
