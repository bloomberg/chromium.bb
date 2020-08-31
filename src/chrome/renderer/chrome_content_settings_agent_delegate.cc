// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_settings_agent_delegate.h"

#include "chrome/common/chrome_features.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/ssl_insecure_content.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/web/web_local_frame.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/renderer_extension_registry.h"
#endif

ChromeContentSettingsAgentDelegate::ChromeContentSettingsAgentDelegate(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<ChromeContentSettingsAgentDelegate>(
          render_frame),
      render_frame_(render_frame) {
  content::RenderFrame* main_frame =
      render_frame->GetRenderView()->GetMainRenderFrame();
  // TODO(nasko): The main frame is not guaranteed to be in the same process
  // with this frame with --site-per-process. This code needs to be updated
  // to handle this case. See https://crbug.com/496670.
  if (main_frame && main_frame != render_frame) {
    auto* parent = ChromeContentSettingsAgentDelegate::Get(main_frame);
    temporarily_allowed_plugins_ = parent->temporarily_allowed_plugins_;
  }
}

ChromeContentSettingsAgentDelegate::~ChromeContentSettingsAgentDelegate() =
    default;

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeContentSettingsAgentDelegate::SetExtensionDispatcher(
    extensions::Dispatcher* extension_dispatcher) {
  DCHECK(!extension_dispatcher_)
      << "SetExtensionDispatcher() should only be called once.";
  extension_dispatcher_ = extension_dispatcher;
}
#endif

bool ChromeContentSettingsAgentDelegate::IsPluginTemporarilyAllowed(
    const std::string& identifier) {
  // If the empty string is in here, it means all plugins are allowed.
  // TODO(bauerb): Remove this once we only pass in explicit identifiers.
  return base::Contains(temporarily_allowed_plugins_, identifier) ||
         base::Contains(temporarily_allowed_plugins_, std::string());
}

bool ChromeContentSettingsAgentDelegate::IsSchemeWhitelisted(
    const std::string& scheme) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return scheme == extensions::kExtensionScheme;
#else
  return false;
#endif
}

base::Optional<bool>
ChromeContentSettingsAgentDelegate::AllowReadFromClipboard() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (current_context && current_context->HasAPIPermission(
                             extensions::APIPermission::kClipboardRead)) {
    return true;
  }
#endif
  return base::nullopt;
}

base::Optional<bool>
ChromeContentSettingsAgentDelegate::AllowWriteToClipboard() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // All blessed extension pages could historically write to the clipboard, so
  // preserve that for compatibility.
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (current_context) {
    if (current_context->effective_context_type() ==
            extensions::Feature::BLESSED_EXTENSION_CONTEXT &&
        !current_context->IsForServiceWorker()) {
      return true;
    }
    if (current_context->HasAPIPermission(
            extensions::APIPermission::kClipboardWrite)) {
      return true;
    }
  }
#endif
  return base::nullopt;
}

base::Optional<bool> ChromeContentSettingsAgentDelegate::AllowMutationEvents() {
  if (IsPlatformApp())
    return false;
  return base::nullopt;
}

base::Optional<bool>
ChromeContentSettingsAgentDelegate::AllowRunningInsecureContent(
    bool allowed_per_settings,
    const blink::WebURL& resource_url) {
  // Note: this implementation is a mirror of
  // Browser::ShouldAllowRunningInsecureContent.
  FilteredReportInsecureContentRan(GURL(resource_url));

  // TODO(crbug.com/987294): We may want to move this logic into
  // ContentSettingsAgentImpl once this feature is launched.
  if (base::FeatureList::IsEnabled(features::kMixedContentSiteSetting)) {
    bool allow = allowed_per_settings;
    auto* agent =
        content_settings::ContentSettingsAgentImpl::Get(render_frame_);
    if (agent->GetContentSettingRules()) {
      auto setting = agent->GetContentSettingFromRules(
          agent->GetContentSettingRules()->mixed_content_rules,
          render_frame_->GetWebFrame(), GURL());
      allow |= (setting == CONTENT_SETTING_ALLOW);
    }
    return allow;
  }

  return base::nullopt;
}

void ChromeContentSettingsAgentDelegate::PassiveInsecureContentFound(
    const blink::WebURL& resource_url) {
  // Note: this implementation is a mirror of
  // Browser::PassiveInsecureContentFound.
  ReportInsecureContent(SslInsecureContentType::DISPLAY);
  FilteredReportInsecureContentDisplayed(GURL(resource_url));
}

bool ChromeContentSettingsAgentDelegate::OnMessageReceived(
    const IPC::Message& message) {
  // Don't swallow LoadBlockedPlugins messages, as they're sent to every
  // blocked plugin.
  IPC_BEGIN_MESSAGE_MAP(ChromeContentSettingsAgentDelegate, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_LoadBlockedPlugins, OnLoadBlockedPlugins)
  IPC_END_MESSAGE_MAP()
  return false;
}

void ChromeContentSettingsAgentDelegate::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  if (render_frame()->GetWebFrame()->Parent() || is_same_document_navigation)
    return;

  temporarily_allowed_plugins_.clear();
}

void ChromeContentSettingsAgentDelegate::OnDestruct() {}

void ChromeContentSettingsAgentDelegate::OnLoadBlockedPlugins(
    const std::string& identifier) {
  temporarily_allowed_plugins_.insert(identifier);
}

bool ChromeContentSettingsAgentDelegate::IsPlatformApp() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  blink::WebSecurityOrigin origin = frame->GetDocument().GetSecurityOrigin();
  const extensions::Extension* extension = GetExtension(origin);
  return extension && extension->is_platform_app();
#else
  return false;
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
const extensions::Extension* ChromeContentSettingsAgentDelegate::GetExtension(
    const blink::WebSecurityOrigin& origin) const {
  if (origin.Protocol().Ascii() != extensions::kExtensionScheme)
    return nullptr;

  const std::string extension_id = origin.Host().Utf8().data();
  if (!extension_dispatcher_->IsExtensionActive(extension_id))
    return nullptr;

  return extensions::RendererExtensionRegistry::Get()->GetByID(extension_id);
}
#endif
