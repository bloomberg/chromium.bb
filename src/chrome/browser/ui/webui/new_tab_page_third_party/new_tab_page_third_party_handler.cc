// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/color_utils.h"

NewTabPageThirdPartyHandler::NewTabPageThirdPartyHandler(
    mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<new_tab_page_third_party::mojom::Page> pending_page,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      page_{std::move(pending_page)},
      receiver_{this, std::move(pending_page_handler)} {
  // Listen for theme installation.
  ThemeServiceFactory::GetForProfile(profile_)->AddObserver(this);
}

NewTabPageThirdPartyHandler::~NewTabPageThirdPartyHandler() {
  ThemeServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void NewTabPageThirdPartyHandler::UpdateTheme() {
  NotifyAboutTheme();
}

void NewTabPageThirdPartyHandler::OnThemeChanged() {
  NotifyAboutTheme();
}

void NewTabPageThirdPartyHandler::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  NotifyAboutTheme();
}

void NewTabPageThirdPartyHandler::NotifyAboutTheme() {
  auto theme = new_tab_page_third_party::mojom::Theme::New();
  auto most_visited = most_visited::mojom::MostVisitedTheme::New();
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_);
  most_visited->background_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_SHORTCUT);
  most_visited->use_white_tile_icon =
      color_utils::IsDark(most_visited->background_color);
  most_visited->use_title_pill = false;
  theme->text_color = theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT);
  most_visited->is_dark = !color_utils::IsDark(theme->text_color);
  theme->color_background = color_utils::SkColorToRgbaString(
      GetThemeColor(webui::GetNativeTheme(web_contents_), theme_provider,
                    ThemeProperties::COLOR_NTP_BACKGROUND));
  if (theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    theme->background_tiling = GetNewTabBackgroundTilingCSS(theme_provider);
    theme->background_position = GetNewTabBackgroundPositionCSS(theme_provider);
    theme->has_custom_background =
        theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND);
    theme->id = profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);
    most_visited->use_title_pill = true;
  }
  theme->most_visited = std::move(most_visited);
  page_->SetTheme(std::move(theme));
}
