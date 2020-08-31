// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_H_
#define CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrame;
}  // namespace content

struct RendererContentSettingRules;

// This client is created on the main renderer thread then passed onto the
// blink's worker thread. For workers created from other workers, Clone()
// is called on the "parent" worker's thread.
class WorkerContentSettingsClient : public blink::WebContentSettingsClient {
 public:
  explicit WorkerContentSettingsClient(content::RenderFrame* render_frame);
  ~WorkerContentSettingsClient() override;

  // WebContentSettingsClient overrides.
  std::unique_ptr<blink::WebContentSettingsClient> Clone() override;
  bool RequestFileSystemAccessSync() override;
  bool AllowIndexedDB() override;
  bool AllowCacheStorage() override;
  bool AllowWebLocks() override;
  bool AllowRunningInsecureContent(bool allowed_per_settings,
                                   const blink::WebURL& url) override;
  bool AllowScriptFromSource(bool enabled_per_settings,
                             const blink::WebURL& script_url) override;
  bool ShouldAutoupgradeMixedContent() override;

 private:
  explicit WorkerContentSettingsClient(
      const WorkerContentSettingsClient& other);
  bool AllowStorageAccess(
      content_settings::mojom::ContentSettingsManager::StorageType
          storage_type);
  void EnsureContentSettingsManager() const;

  // Loading document context for this worker.
  bool is_unique_origin_ = false;
  url::Origin document_origin_;
  GURL site_for_cookies_;
  url::Origin top_frame_origin_;
  bool allow_running_insecure_content_;
  const int32_t render_frame_id_;
  const RendererContentSettingRules* content_setting_rules_;

  // Because instances of this class are created on the parent's thread (i.e,
  // on the renderer main thread or on the thread of the parent worker), it is
  // necessary to lazily bind the |content_settings_manager_| remote. The
  // pending remote is initialized on the parent thread and then the remote is
  // bound when needed on the worker's thread.
  mutable mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
      pending_content_settings_manager_;
  mutable mojo::Remote<content_settings::mojom::ContentSettingsManager>
      content_settings_manager_;

  DISALLOW_ASSIGN(WorkerContentSettingsClient);
};

#endif  // CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_H_
