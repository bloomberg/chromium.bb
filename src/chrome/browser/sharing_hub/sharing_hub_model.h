// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_HUB_SHARING_HUB_MODEL_H_
#define CHROME_BROWSER_SHARING_HUB_SHARING_HUB_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/share/core/share_targets_observer.h"
#include "ui/gfx/image/image_skia.h"

class GURL;
class Profile;

namespace sharing {
namespace mojom {
class ShareTargets;
}  // namespace mojom
}  // namespace sharing

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace sharing_hub {

struct SharingHubAction {
  SharingHubAction(int command_id,
                   std::u16string title,
                   const gfx::VectorIcon* icon,
                   bool is_first_party,
                   gfx::ImageSkia third_party_icon,
                   std::string feature_name_for_metrics);
  SharingHubAction(const SharingHubAction&);
  SharingHubAction& operator=(const SharingHubAction&);
  SharingHubAction(SharingHubAction&&);
  SharingHubAction& operator=(SharingHubAction&&);
  ~SharingHubAction() = default;
  int command_id;
  std::u16string title;
  raw_ptr<const gfx::VectorIcon> icon;
  bool is_first_party;
  gfx::ImageSkia third_party_icon;
  std::string feature_name_for_metrics;
};

// The Sharing Hub model contains a list of first and third party actions.
// This object should only be accessed from one thread, which is usually the
// main thread.
class SharingHubModel : public sharing::ShareTargetsObserver {
 public:
  explicit SharingHubModel(content::BrowserContext* context);
  SharingHubModel(const SharingHubModel&) = delete;
  SharingHubModel& operator=(const SharingHubModel&) = delete;
  ~SharingHubModel() override;

  // Populates the vector with first party Sharing Hub actions, ordered by
  // appearance in the dialog. Some actions (i.e. send tab to self) may not be
  // shown for some URLs.
  void GetFirstPartyActionList(content::WebContents* web_contents,
                               std::vector<SharingHubAction>* list);
  // Populates the vector with third party Sharing Hub actions, ordered by
  // appearance in the dialog.
  void GetThirdPartyActionList(std::vector<SharingHubAction>* list);

  // Executes the third party action indicated by |id|, i.e. opens a popup to
  // the corresponding webpage. The |url| is the URL to share, and the |title|
  // is the title (if there is one) of the shared URL.
  void ExecuteThirdPartyAction(Profile* profile,
                               const GURL& url,
                               const std::u16string& title,
                               int id);

  // Convenience wrapper around the above when sharing a WebContents. This
  // extracts the title and URL to share from the provided WebContents.
  void ExecuteThirdPartyAction(content::WebContents* contents, int id);

  // sharing::ShareTargetsObserver implementation.
  void OnShareTargetsUpdated(
      std::unique_ptr<sharing::mojom::ShareTargets> ShareTarget) override;

 private:
  void PopulateFirstPartyActions();
  void PopulateThirdPartyActions();

  bool DoShowSendTabToSelfForWebContents(content::WebContents* web_contents);

  // A list of Sharing Hub first party actions in order in which they appear.
  std::vector<SharingHubAction> first_party_action_list_;
  // A list of Sharing Hub third party actions in order in which they appear.
  std::vector<SharingHubAction> third_party_action_list_;

  // A list of third party action URLs mapped to action id.
  std::map<int, GURL> third_party_action_urls_;

  raw_ptr<content::BrowserContext> context_;

  std::unique_ptr<sharing::mojom::ShareTargets> third_party_targets_;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_SHARING_HUB_SHARING_HUB_MODEL_H_
