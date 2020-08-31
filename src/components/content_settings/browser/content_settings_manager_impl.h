// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_CONTENT_SETTINGS_MANAGER_IMPL_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_CONTENT_SETTINGS_MANAGER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

namespace content_settings {
class CookieSettings;
}  // namespace content_settings

namespace content_settings {

class ContentSettingsManagerImpl
    : public content_settings::mojom::ContentSettingsManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Gets cookie settings for this browser context.
    virtual scoped_refptr<CookieSettings> GetCookieSettings(
        content::BrowserContext* browser_context) = 0;

    // Allows delegate to override AllowStorageAccess(). If the delegate returns
    // true here, the default logic will be bypassed.
    virtual bool AllowStorageAccess(
        int render_process_id,
        int render_frame_id,
        StorageType storage_type,
        const GURL& url,
        bool allowed,
        base::OnceCallback<void(bool)>* callback) = 0;

    // Returns a new instance of this delegate.
    virtual std::unique_ptr<Delegate> Clone() = 0;
  };

  ~ContentSettingsManagerImpl() override;

  static void Create(
      content::RenderProcessHost* render_process_host,
      mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
          receiver,
      std::unique_ptr<Delegate> delegate);

  // mojom::ContentSettingsManager methods:
  void Clone(
      mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
          receiver) override;
  void AllowStorageAccess(int32_t render_frame_id,
                          StorageType storage_type,
                          const url::Origin& origin,
                          const GURL& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          base::OnceCallback<void(bool)> callback) override;
  void OnContentBlocked(int32_t render_frame_id,
                        ContentSettingsType type) override;

 private:
  ContentSettingsManagerImpl(content::RenderProcessHost* render_process_host,
                             std::unique_ptr<Delegate> delegate);
  ContentSettingsManagerImpl(const ContentSettingsManagerImpl& other);

  std::unique_ptr<Delegate> delegate_;

  // Use these IDs to hold a weak reference back to the RenderFrameHost.
  const int render_process_id_;

  // Used to look up storage permissions.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_CONTENT_SETTINGS_MANAGER_IMPL_H_
