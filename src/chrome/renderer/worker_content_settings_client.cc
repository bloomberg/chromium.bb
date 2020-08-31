// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/worker_content_settings_client.h"

#include "base/memory/ptr_util.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/renderer/content_settings_agent_impl.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/origin.h"

WorkerContentSettingsClient::WorkerContentSettingsClient(
    content::RenderFrame* render_frame)
    : render_frame_id_(render_frame->GetRoutingID()) {
  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  const blink::WebDocument& document = frame->GetDocument();
  if (document.GetSecurityOrigin().IsOpaque() ||
      frame->Top()->GetSecurityOrigin().IsOpaque())
    is_unique_origin_ = true;

  document_origin_ = document.GetSecurityOrigin();
  site_for_cookies_ = document.SiteForCookies().RepresentativeUrl();
  top_frame_origin_ = document.TopFrameOrigin();

  content::ChildThread::Get()->BindHostReceiver(
      pending_content_settings_manager_.InitWithNewPipeAndPassReceiver());

  content_settings::ContentSettingsAgentImpl* agent =
      content_settings::ContentSettingsAgentImpl::Get(render_frame);
  allow_running_insecure_content_ = agent->allow_running_insecure_content();
  content_setting_rules_ = agent->GetContentSettingRules();
}

WorkerContentSettingsClient::WorkerContentSettingsClient(
    const WorkerContentSettingsClient& other)
    : is_unique_origin_(other.is_unique_origin_),
      document_origin_(other.document_origin_),
      site_for_cookies_(other.site_for_cookies_),
      top_frame_origin_(other.top_frame_origin_),
      allow_running_insecure_content_(other.allow_running_insecure_content_),
      render_frame_id_(other.render_frame_id_),
      content_setting_rules_(other.content_setting_rules_) {
  other.EnsureContentSettingsManager();
  other.content_settings_manager_->Clone(
      pending_content_settings_manager_.InitWithNewPipeAndPassReceiver());
}

WorkerContentSettingsClient::~WorkerContentSettingsClient() {}

std::unique_ptr<blink::WebContentSettingsClient>
WorkerContentSettingsClient::Clone() {
  return base::WrapUnique(new WorkerContentSettingsClient(*this));
}

bool WorkerContentSettingsClient::RequestFileSystemAccessSync() {
  return AllowStorageAccess(content_settings::mojom::ContentSettingsManager::
                                StorageType::FILE_SYSTEM);
}

bool WorkerContentSettingsClient::AllowIndexedDB() {
  return AllowStorageAccess(
      content_settings::mojom::ContentSettingsManager::StorageType::INDEXED_DB);
}

bool WorkerContentSettingsClient::AllowCacheStorage() {
  return AllowStorageAccess(
      content_settings::mojom::ContentSettingsManager::StorageType::CACHE);
}

bool WorkerContentSettingsClient::AllowWebLocks() {
  return AllowStorageAccess(
      content_settings::mojom::ContentSettingsManager::StorageType::WEB_LOCKS);
}

bool WorkerContentSettingsClient::AllowRunningInsecureContent(
    bool allowed_per_settings,
    const blink::WebURL& url) {
  if (!allow_running_insecure_content_ && !allowed_per_settings) {
    EnsureContentSettingsManager();
    content_settings_manager_->OnContentBlocked(
        render_frame_id_, ContentSettingsType::MIXEDSCRIPT);
    return false;
  }

  return true;
}

bool WorkerContentSettingsClient::AllowScriptFromSource(
    bool enabled_per_settings,
    const blink::WebURL& script_url) {
  bool allow = enabled_per_settings;
  if (allow && content_setting_rules_) {
    GURL top_frame_origin_url = top_frame_origin_.GetURL();
    for (const auto& rule : content_setting_rules_->script_rules) {
      if (rule.primary_pattern.Matches(top_frame_origin_url) &&
          rule.secondary_pattern.Matches(script_url)) {
        allow = rule.GetContentSetting() != CONTENT_SETTING_BLOCK;
        break;
      }
    }
  }

  if (!allow) {
    EnsureContentSettingsManager();
    content_settings_manager_->OnContentBlocked(
        render_frame_id_, ContentSettingsType::JAVASCRIPT);
    return false;
  }

  return true;
}

bool WorkerContentSettingsClient::ShouldAutoupgradeMixedContent() {
  if (content_setting_rules_) {
    for (const auto& rule : content_setting_rules_->mixed_content_rules) {
      if (rule.primary_pattern.Matches(top_frame_origin_.GetURL())) {
        return rule.GetContentSetting() != CONTENT_SETTING_ALLOW;
      }
    }
  }
  return false;
}

bool WorkerContentSettingsClient::AllowStorageAccess(
    content_settings::mojom::ContentSettingsManager::StorageType storage_type) {
  if (is_unique_origin_)
    return false;

  EnsureContentSettingsManager();

  bool result = false;
  content_settings_manager_->AllowStorageAccess(
      render_frame_id_, storage_type, document_origin_, site_for_cookies_,
      top_frame_origin_, &result);
  return result;
}

void WorkerContentSettingsClient::EnsureContentSettingsManager() const {
  // Lazily bind |content_settings_manager_| so it is bound on the right thread.
  if (content_settings_manager_)
    return;
  DCHECK(pending_content_settings_manager_);
  content_settings_manager_.Bind(std::move(pending_content_settings_manager_));
}
