// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_MIXED_CONTENT_SETTINGS_TAB_HELPER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_MIXED_CONTENT_SETTINGS_TAB_HELPER_H_

#include <map>
#include <memory>
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Controls mixed content related settings for the associated WebContents,
// working as the browser version of the mixed content state kept by
// ContentSettingsObserver in the renderer.
class MixedContentSettingsTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MixedContentSettingsTabHelper> {
 public:
  ~MixedContentSettingsTabHelper() override;

  // Enables running active mixed content resources in the associated
  // WebContents/tab. This will stick around as long as the main frame's
  // RenderFrameHost stays the same. When the RenderFrameHost changes, we're
  // back to the default (mixed content resources are not allowed to run).
  void AllowRunningOfInsecureContent();

  bool IsRunningInsecureContentAllowed();

 private:
  friend class content::WebContentsUserData<MixedContentSettingsTabHelper>;

  explicit MixedContentSettingsTabHelper(content::WebContents* tab);

  // content::WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // TODO(crbug.com/1071232): When RenderDocument is implemented, make this a
  // RenderDocumentHostUserData.
  class PageSettings {
   public:
    explicit PageSettings(content::RenderFrameHost* render_frame_host);
    PageSettings(const PageSettings&) = delete;
    void operator=(const PageSettings&) = delete;

    void AllowRunningOfInsecureContent();

    bool is_running_insecure_content_allowed() {
      return is_running_insecure_content_allowed_;
    }

   private:
    bool is_running_insecure_content_allowed_ = false;
  };

  std::map<content::RenderFrameHost*, std::unique_ptr<PageSettings>> settings_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(MixedContentSettingsTabHelper);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_MIXED_CONTENT_SETTINGS_TAB_HELPER_H_
