// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_UI_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_UI_VIEWS_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/same_origin_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/image_model.h"

namespace content {
class WebContents;
}
namespace infobars {
class InfoBar;
}

class Profile;

class TabSharingUIViews : public TabSharingUI,
                          public BrowserListObserver,
                          public TabStripModelObserver,
                          public infobars::InfoBarManager::Observer,
                          public content::WebContentsObserver {
 public:
  TabSharingUIViews(content::GlobalRenderFrameHostId capturer,
                    const content::DesktopMediaID& media_id,
                    std::u16string app_name,
                    bool region_capture_capable,
                    bool favicons_used_for_switch_to_tab_button);
  ~TabSharingUIViews() override;

  // MediaStreamUI:
  // Called when tab sharing has started. Creates infobars on all tabs.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override;

  // TabSharingUI:
  // Runs |source_callback_| to start sharing the tab containing |infobar|.
  // Removes infobars on all tabs; OnStarted() will recreate the infobars with
  // updated title and buttons.
  void StartSharing(infobars::InfoBar* infobar) override;

  // Runs |stop_callback_| to stop sharing |shared_tab_|. Removes infobars on
  // all tabs.
  void StopSharing() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;
  // DidUpdateFaviconURL() is not overridden. We wait until
  // FaviconPeriodicUpdate() before updating the favicon. A captured tab can
  // toggle its favicon back and forth at an arbitrary rate, but we implicitly
  // rate-limit our response.

 private:
  friend class TabSharingUIViewsBrowserTest;

  enum class TabCaptureUpdate {
    kCaptureAdded,
    kCaptureRemoved,
    kCapturedVisibilityUpdated
  };

  void CreateInfobarsForAllTabs();
  void CreateInfobarForWebContents(content::WebContents* contents);
  void RemoveInfobarsForAllTabs();

  void CreateTabCaptureIndicator();

  // Periodically checks for changes that would require the infobar to be
  // recreated, such as a favicon change.
  // Consult |share_session_seq_num_| for |share_session_seq_num|'s meaning.
  void FaviconPeriodicUpdate(size_t share_session_seq_num);

  void RefreshFavicons();

  void MaybeUpdateFavicon(content::WebContents* focus_target,
                          absl::optional<uint32_t>* current_hash,
                          content::WebContents* infobar_owner);

  ui::ImageModel TabFavicon(content::WebContents* web_contents) const;
  ui::ImageModel TabFavicon(content::GlobalRenderFrameHostId rfh_id) const;

  void SetTabFaviconForTesting(content::WebContents* web_contents,
                               const ui::ImageModel& favicon);

  void StopCaptureDueToPolicy(content::WebContents* contents);

  void UpdateTabCaptureData(content::WebContents* contents,
                            TabCaptureUpdate update);

  std::map<content::WebContents*, infobars::InfoBar*> infobars_;
  std::map<content::WebContents*, std::unique_ptr<SameOriginObserver>>
      same_origin_observers_;
  const content::GlobalRenderFrameHostId capturer_;
  const url::Origin capturer_origin_;
  const bool can_focus_capturer_;
  const bool capturer_restricted_to_same_origin_ = false;
  content::DesktopMediaID shared_tab_media_id_;
  const std::u16string app_name_;
  raw_ptr<content::WebContents> shared_tab_;
  std::unique_ptr<SameOriginObserver> shared_tab_origin_observer_;
  std::u16string shared_tab_name_;
  const bool is_self_capture_;
  const bool region_capture_capable_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<content::MediaStreamUI> tab_capture_indicator_ui_;

  // FaviconPeriodicUpdate() runs on a delayed task which re-posts itself.
  // The first task is associated with |share_session_seq_num_|, then all
  // repetitions of the task are associated with that value.
  // When |share_session_seq_num_| is incremented, all previously scheduled
  // tasks are invalidated, thereby ensuring that no more than one "live"
  // FaviconPeriodicUpdate() task can exist at any given moment.
  size_t share_session_seq_num_ = 0;

  content::MediaStreamUI::SourceCallback source_callback_;
  base::OnceClosure stop_callback_;

  // TODO(crbug.com/1224363): Re-enable favicons by default or drop the code.
  const bool favicons_used_for_switch_to_tab_button_;

  absl::optional<uint32_t> capturer_favicon_hash_;
  absl::optional<uint32_t> captured_favicon_hash_;

  std::map<content::WebContents*, ui::ImageModel>
      favicon_overrides_for_testing_;

  base::WeakPtrFactory<TabSharingUIViews> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_UI_VIEWS_H_
