// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_util.h"

#include "base/containers/cxx20_erase.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/resources/grit/webui_resources_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/enterprise_util.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/widget.h"
#endif  // defined(TOOLKIT_VIEWS)

namespace webui {

void SetJSModuleDefaults(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test "
      "chrome://webui-test 'self';");
  // TODO(crbug.com/1098690): Trusted Type Polymer
  source->DisableTrustedTypesCSP();
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
}

void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const ResourcePath> resources,
                          int default_resource) {
  SetJSModuleDefaults(source);
  source->AddResourcePaths(resources);
  source->AddResourcePath("", default_resource);
}

void AddLocalizedString(content::WebUIDataSource* source,
                        const std::string& message,
                        int id) {
  std::u16string str = l10n_util::GetStringUTF16(id);
  base::Erase(str, '&');
  source->AddString(message, str);
}

bool IsEnterpriseManaged() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->IsDeviceEnterpriseManaged();
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return base::IsMachineExternallyManaged();
#else
  return false;
#endif
}

#if defined(TOOLKIT_VIEWS)

namespace {
const ui::ThemeProvider* g_theme_provider_for_testing = nullptr;
}  // namespace

ui::NativeTheme* GetNativeTheme(content::WebContents* web_contents) {
  ui::NativeTheme* native_theme = nullptr;

  if (web_contents) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

    if (browser) {
      // Find for WebContents hosted in a tab.
      native_theme = browser->window()->GetNativeTheme();
    }

    if (!native_theme) {
      // Find for WebContents hosted in a widget, but not directly in a
      // Browser. e.g. Tab Search, Read Later.
      views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
          web_contents->GetContentNativeView());
      if (widget)
        native_theme = widget->GetNativeTheme();
    }
  }

  if (!native_theme) {
    // Find for isolated WebContents, e.g. in tests.
    // Or when |web_contents| is nullptr, because the renderer is not ready.
    // TODO(crbug/1056916): Remove global accessor to NativeTheme.
    native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  }

  return native_theme;
}

const ui::ThemeProvider* GetThemeProvider(content::WebContents* web_contents) {
  if (g_theme_provider_for_testing)
    return g_theme_provider_for_testing;

  auto* browser_window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents);

  if (browser_window)
    return browser_window->GetThemeProvider();

  // Fallback: get the theme provider from the last created browser.
  // This is used in newly created tabs, e.g. NewTabPageUI. They need access to
  // the theme browser before the WebContents is attached to a browser window.
  // TODO(crbug.com/1298767): Remove this fallback by associating the
  // WebContents during navigation.
  BrowserList* browser_list = BrowserList::GetInstance();
  const Browser* browser = browser_list->empty()
                               ? nullptr
                               : *std::prev(BrowserList::GetInstance()->end());
  return browser ? browser->window()->GetThemeProvider() : nullptr;
}

void SetThemeProviderForTesting(const ui::ThemeProvider* theme_provider) {
  g_theme_provider_for_testing = theme_provider;
}

#endif  // defined(TOOLKIT_VIEWS)

}  // namespace webui
