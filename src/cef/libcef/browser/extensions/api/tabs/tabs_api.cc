// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/extensions/api/tabs/tabs_api.h"

#include "libcef/browser/extensions/extension_web_contents_observer.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/page/page_zoom.h"

using zoom::ZoomController;

namespace extensions {
namespace cef {

namespace keys = extensions::tabs_constants;
namespace tabs = api::tabs;

using api::extension_types::InjectDetails;

namespace {

const char kNotImplementedError[] = "Not implemented";

void ZoomModeToZoomSettings(zoom::ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  switch (zoom_mode) {
    case zoom::ZoomController::ZOOM_MODE_DEFAULT:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN;
      break;
    case zoom::ZoomController::ZOOM_MODE_ISOLATED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case zoom::ZoomController::ZOOM_MODE_MANUAL:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_MANUAL;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case zoom::ZoomController::ZOOM_MODE_DISABLED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_DISABLED;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
  }
}

template <typename T>
void AssignOptionalValue(const std::unique_ptr<T>& source,
                         std::unique_ptr<T>& destination) {
  if (source.get()) {
    destination.reset(new T(*source));
  }
}

}  // namespace

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  return RespondNow(Error(kNotImplementedError));
}

TabsCreateFunction::TabsCreateFunction() : cef_details_(this) {}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::unique_ptr<tabs::Create::Params> params(
      tabs::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CefExtensionFunctionDetails::OpenTabParams options;
  AssignOptionalValue(params->create_properties.window_id, options.window_id);
  AssignOptionalValue(params->create_properties.opener_tab_id,
                      options.opener_tab_id);
  AssignOptionalValue(params->create_properties.selected, options.active);
  // The 'active' property has replaced the 'selected' property.
  AssignOptionalValue(params->create_properties.active, options.active);
  AssignOptionalValue(params->create_properties.pinned, options.pinned);
  AssignOptionalValue(params->create_properties.index, options.index);
  AssignOptionalValue(params->create_properties.url, options.url);

  std::string error;
  std::unique_ptr<base::DictionaryValue> result(
      cef_details_.OpenTab(options, user_gesture(), &error));
  if (!result)
    return RespondNow(Error(error));

  // Return data about the newly created tab.
  return RespondNow(
      has_callback()
          ? OneArgument(base::Value::FromUniquePtrValue(std::move(result)))
          : NoArguments());
}

BaseAPIFunction::BaseAPIFunction() : cef_details_(this) {}

content::WebContents* BaseAPIFunction::GetWebContents(int tab_id) {
  // Find a browser that we can access, or set |error_| and return nullptr.
  CefRefPtr<AlloyBrowserHostImpl> browser =
      cef_details_.GetBrowserForTabIdFirstTime(tab_id, &error_);
  if (!browser)
    return nullptr;

  return browser->web_contents();
}

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::unique_ptr<tabs::Update::Params> params(
      tabs::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  tab_id_ = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id_);
  if (!web_contents)
    return RespondNow(Error(std::move(error_)));

  web_contents_ = web_contents;

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url is different.
  if (params->update_properties.url.get()) {
    std::string updated_url = *params->update_properties.url;
    if (!UpdateURL(updated_url, tab_id_, &error_))
      return RespondNow(Error(std::move(error_)));
  }

  bool active = false;
  // TODO(rafaelw): Setting |active| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (params->update_properties.selected.get())
    active = *params->update_properties.selected;

  // The 'active' property has replaced 'selected'.
  if (params->update_properties.active.get())
    active = *params->update_properties.active;

  if (active) {
    // TODO: Activate the tab at |tab_id_|.
    NOTIMPLEMENTED();
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
  }

  if (params->update_properties.highlighted.get() &&
      *params->update_properties.highlighted) {
    // TODO: Highlight the tab at |tab_id_|.
    NOTIMPLEMENTED();
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
  }

  if (params->update_properties.pinned.get() &&
      *params->update_properties.pinned) {
    // TODO: Pin the tab at |tab_id_|.
    NOTIMPLEMENTED();
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
  }

  if (params->update_properties.muted.get()) {
    // TODO: Mute/unmute the tab at |tab_id_|.
    NOTIMPLEMENTED();
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        tabs_constants::kCannotUpdateMuteCaptured,
        base::NumberToString(tab_id_))));
  }

  if (params->update_properties.opener_tab_id.get()) {
    int opener_id = *params->update_properties.opener_tab_id;
    if (opener_id == tab_id_)
      return RespondNow(Error("Cannot set a tab's opener to itself."));

    // TODO: Set the opener for the tab at |tab_id_|.
    NOTIMPLEMENTED();
    return RespondNow(Error(tabs_constants::kTabStripNotEditableError));
  }

  if (params->update_properties.auto_discardable.get()) {
    // TODO: Set auto-discardable state for the tab at |tab_id_|.
    NOTIMPLEMENTED();
  }

  return RespondNow(GetResult());
}

bool TabsUpdateFunction::UpdateURL(const std::string& url_string,
                                   int tab_id,
                                   std::string* error) {
  GURL url;
  if (!ExtensionTabUtil::PrepareURLForNavigation(url_string, extension(), &url,
                                                 error)) {
    return false;
  }

  const bool is_javascript_scheme = url.SchemeIs(url::kJavaScriptScheme);
  // JavaScript URLs are forbidden in chrome.tabs.update().
  if (is_javascript_scheme) {
    *error = tabs_constants::kJavaScriptUrlsNotAllowedInTabsUpdate;
    return false;
  }

  content::NavigationController::LoadURLParams load_params(url);

  // Treat extension-initiated navigations as renderer-initiated so that the URL
  // does not show in the omnibox until it commits.  This avoids URL spoofs
  // since URLs can be opened on behalf of untrusted content.
  load_params.is_renderer_initiated = true;
  // All renderer-initiated navigations need to have an initiator origin.
  load_params.initiator_origin = extension()->origin();
  // |source_site_instance| needs to be set so that a renderer process
  // compatible with |initiator_origin| is picked by Site Isolation.
  load_params.source_site_instance = content::SiteInstance::CreateForURL(
      web_contents_->GetBrowserContext(),
      load_params.initiator_origin->GetURL());

  // Marking the navigation as initiated via an API means that the focus
  // will stay in the omnibox - see https://crbug.com/1085779.
  load_params.transition_type = ui::PAGE_TRANSITION_FROM_API;

  web_contents_->GetController().LoadURLWithParams(load_params);

  DCHECK_EQ(url,
            web_contents_->GetController().GetPendingEntry()->GetVirtualURL());

  return true;
}

ExtensionFunction::ResponseValue TabsUpdateFunction::GetResult() {
  if (!has_callback())
    return NoArguments();

  return ArgumentList(tabs::Get::Results::Create(*cef_details_.CreateTabObject(
      AlloyBrowserHostImpl::GetBrowserForContents(web_contents_),
      /*opener_browser_id=*/-1, /*active=*/true, tab_id_)));
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : cef_details_(this), execute_tab_id_(-1) {}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

ExecuteCodeFunction::InitResult ExecuteCodeInTabFunction::Init() {
  if (init_result_)
    return init_result_.value();

  // |tab_id| is optional so it's ok if it's not there.
  int tab_id = -1;
  if (args_->GetInteger(0, &tab_id) && tab_id < 0)
    return set_init_result(VALIDATION_FAILURE);

  // |details| are not optional.
  base::DictionaryValue* details_value = nullptr;
  if (!args_->GetDictionary(1, &details_value))
    return set_init_result(VALIDATION_FAILURE);
  std::unique_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return set_init_result(VALIDATION_FAILURE);

  // Find a browser that we can access, or fail with error.
  std::string error;
  CefRefPtr<AlloyBrowserHostImpl> browser =
      cef_details_.GetBrowserForTabIdFirstTime(tab_id, &error);
  if (!browser)
    return set_init_result_error(error);

  execute_tab_id_ = browser->GetIdentifier();
  details_ = std::move(details);
  set_host_id(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension()->id()));
  return set_init_result(SUCCESS);
}

bool ExecuteCodeInTabFunction::ShouldInsertCSS() const {
  return false;
}

bool ExecuteCodeInTabFunction::ShouldRemoveCSS() const {
  return false;
}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage(std::string* error) {
  CHECK_GE(execute_tab_id_, 0);

  CefRefPtr<AlloyBrowserHostImpl> browser =
      cef_details_.GetBrowserForTabIdAgain(execute_tab_id_, error);
  if (!browser)
    return false;

  int frame_id = details_->frame_id ? *details_->frame_id
                                    : ExtensionApiFrameIdMap::kTopFrameId;
  content::RenderFrameHost* rfh =
      ExtensionApiFrameIdMap::GetRenderFrameHostById(browser->web_contents(),
                                                     frame_id);
  if (!rfh) {
    *error = ErrorUtils::FormatErrorMessage(
        keys::kFrameNotFoundError, base::NumberToString(frame_id),
        base::NumberToString(execute_tab_id_));
    return false;
  }

  // Content scripts declared in manifest.json can access frames at about:-URLs
  // if the extension has permission to access the frame's origin, so also allow
  // programmatic content scripts at about:-URLs for allowed origins.
  GURL effective_document_url(rfh->GetLastCommittedURL());
  bool is_about_url = effective_document_url.SchemeIs(url::kAboutScheme);
  if (is_about_url && details_->match_about_blank &&
      *details_->match_about_blank) {
    effective_document_url = GURL(rfh->GetLastCommittedOrigin().Serialize());
  }

  if (!effective_document_url.is_valid()) {
    // Unknown URL, e.g. because no load was committed yet. Allow for now, the
    // renderer will check again and fail the injection if needed.
    return true;
  }

  // NOTE: This can give the wrong answer due to race conditions, but it is OK,
  // we check again in the renderer.
  if (!extension()->permissions_data()->CanAccessPage(effective_document_url,
                                                      execute_tab_id_, error)) {
    if (is_about_url &&
        extension()->permissions_data()->active_permissions().HasAPIPermission(
            mojom::APIPermissionID::kTab)) {
      *error = ErrorUtils::FormatErrorMessage(
          manifest_errors::kCannotAccessAboutUrl,
          rfh->GetLastCommittedURL().spec(),
          rfh->GetLastCommittedOrigin().Serialize());
    }
    return false;
  }

  return true;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor(
    std::string* error) {
  CHECK_GE(execute_tab_id_, 0);

  CefRefPtr<AlloyBrowserHostImpl> browser =
      cef_details_.GetBrowserForTabIdAgain(execute_tab_id_, error);
  if (!browser)
    return nullptr;

  return CefExtensionWebContentsObserver::FromWebContents(
             browser->web_contents())
      ->script_executor();
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  return false;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  return GURL::EmptyGURL();
}

bool ExecuteCodeInTabFunction::LoadFile(const std::string& file,
                                        std::string* error) {
  if (cef_details_.LoadFile(
          file, base::BindOnce(&ExecuteCodeInTabFunction::LoadFileComplete,
                               this, file))) {
    return true;
  }

  // Default handling.
  return ExecuteCodeFunction::LoadFile(file, error);
}

void ExecuteCodeInTabFunction::LoadFileComplete(
    const std::string& file,
    std::unique_ptr<std::string> data) {
  const bool success = !!data.get();
  DidLoadAndLocalizeFile(file, success, std::move(data));
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

bool TabsRemoveCSSFunction::ShouldRemoveCSS() const {
  return true;
}

ExtensionFunction::ResponseAction TabsSetZoomFunction::Run() {
  std::unique_ptr<tabs::SetZoom::Params> params(
      tabs::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return RespondNow(Error(std::move(error_)));

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error_))
    return RespondNow(Error(std::move(error_)));

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  double zoom_level =
      params->zoom_factor > 0
          ? blink::PageZoomFactorToZoomLevel(params->zoom_factor)
          : zoom_controller->GetDefaultZoomLevel();

  auto client = base::MakeRefCounted<ExtensionZoomRequestClient>(extension());
  if (!zoom_controller->SetZoomLevelByClient(zoom_level, client)) {
    // Tried to zoom a tab in disabled mode.
    return RespondNow(Error(tabs_constants::kCannotZoomDisabledTabError));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomFunction::Run() {
  std::unique_ptr<tabs::GetZoom::Params> params(
      tabs::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return RespondNow(Error(std::move(error_)));

  double zoom_level =
      zoom::ZoomController::FromWebContents(web_contents)->GetZoomLevel();
  double zoom_factor = blink::PageZoomLevelToZoomFactor(zoom_level);

  return RespondNow(ArgumentList(tabs::GetZoom::Results::Create(zoom_factor)));
}

ExtensionFunction::ResponseAction TabsSetZoomSettingsFunction::Run() {
  using api::tabs::ZoomSettings;

  std::unique_ptr<tabs::SetZoomSettings::Params> params(
      tabs::SetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return RespondNow(Error(std::move(error_)));

  GURL url(web_contents->GetVisibleURL());
  std::string error;
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error_))
    return RespondNow(Error(std::move(error_)));

  // "per-origin" scope is only available in "automatic" mode.
  if (params->zoom_settings.scope == tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_AUTOMATIC &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_NONE) {
    return RespondNow(Error(tabs_constants::kPerOriginOnlyInAutomaticError));
  }

  // Determine the correct internal zoom mode to set |web_contents| to from the
  // user-specified |zoom_settings|.
  ZoomController::ZoomMode zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_settings.mode) {
    case tabs::ZOOM_SETTINGS_MODE_NONE:
    case tabs::ZOOM_SETTINGS_MODE_AUTOMATIC:
      switch (params->zoom_settings.scope) {
        case tabs::ZOOM_SETTINGS_SCOPE_NONE:
        case tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN:
          zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
          break;
        case tabs::ZOOM_SETTINGS_SCOPE_PER_TAB:
          zoom_mode = ZoomController::ZOOM_MODE_ISOLATED;
      }
      break;
    case tabs::ZOOM_SETTINGS_MODE_MANUAL:
      zoom_mode = ZoomController::ZOOM_MODE_MANUAL;
      break;
    case tabs::ZOOM_SETTINGS_MODE_DISABLED:
      zoom_mode = ZoomController::ZOOM_MODE_DISABLED;
  }

  ZoomController::FromWebContents(web_contents)->SetZoomMode(zoom_mode);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetZoomSettingsFunction::Run() {
  std::unique_ptr<tabs::GetZoomSettings::Params> params(
      tabs::GetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return RespondNow(Error(std::move(error_)));
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  ZoomController::ZoomMode zoom_mode = zoom_controller->zoom_mode();
  api::tabs::ZoomSettings zoom_settings;
  ZoomModeToZoomSettings(zoom_mode, &zoom_settings);
  zoom_settings.default_zoom_factor = std::make_unique<double>(
      blink::PageZoomLevelToZoomFactor(zoom_controller->GetDefaultZoomLevel()));

  return RespondNow(
      ArgumentList(api::tabs::GetZoomSettings::Results::Create(zoom_settings)));
}

}  // namespace cef
}  // namespace extensions
