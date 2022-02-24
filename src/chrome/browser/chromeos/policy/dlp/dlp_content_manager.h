// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/media_stream_request.h"
#include "url/gurl.h"

namespace content {
struct DesktopMediaID;
class WebContents;
}  // namespace content

namespace policy {

class DlpReportingManager;

class DlpWarnNotifier;

// System-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
// If any confidential WebContents is visible, the corresponding restrictions
// will be enforced according to the current enterprise policy.
class DlpContentManager : public DlpContentObserver,
                          public BrowserListObserver,
                          public TabStripModelObserver {
 public:
  DlpContentManager(const DlpContentManager&) = delete;
  DlpContentManager& operator=(const DlpContentManager&) = delete;

  // Returns platform-specific implementation of the class. Never returns
  // nullptr.
  static DlpContentManager* Get();

  // Returns which restrictions are applied to the |web_contents| according to
  // the policy.
  DlpContentRestrictionSet GetConfidentialRestrictions(
      content::WebContents* web_contents) const;

  // Returns whether screenshare should be blocked for the specified
  // WebContents.
  bool IsScreenShareBlocked(content::WebContents* web_contents) const;

  // Checks whether printing of |web_contents| is restricted or not advised.
  // Depending on the result, calls |callback| and passes an indicator whether
  // to proceed or not.
  void CheckPrintingRestriction(content::WebContents* web_contents,
                                OnDlpRestrictionCheckedCallback callback);

  // Returns whether screenshots should be restricted for extensions API.
  virtual bool IsScreenshotApiRestricted(content::WebContents* web_contents);

  // Checks whether screen sharing of content from the |media_id| source with
  // application |application_name| is restricted or not advised. Depending on
  // the result, calls |callback| and passes an indicator whether to proceed or
  // not.
  virtual void CheckScreenShareRestriction(
      const content::DesktopMediaID& media_id,
      const std::u16string& application_title,
      OnDlpRestrictionCheckedCallback callback) = 0;

  // Called when screen share is started.
  // |state_change_callback| will be called when restricted content will appear
  // or disappear in the captured area to pause/resume the share.
  // |source_callback| will be called only to update the source for a tab share
  // before resuming the capture.
  // |stop_callback| will be called after a user dismisses a warning.
  virtual void OnScreenShareStarted(
      const std::string& label,
      std::vector<content::DesktopMediaID> screen_share_ids,
      const std::u16string& application_title,
      base::RepeatingClosure stop_callback,
      content::MediaStreamUI::StateChangeCallback state_change_callback,
      content::MediaStreamUI::SourceCallback source_callback) = 0;

  // Called when screen share is stopped.
  virtual void OnScreenShareStopped(
      const std::string& label,
      const content::DesktopMediaID& media_id) = 0;

  void AddObserver(DlpContentManagerObserver* observer,
                   DlpContentRestriction restriction);

  void RemoveObserver(const DlpContentManagerObserver* observer,
                      DlpContentRestriction restriction);

 protected:
  friend class DlpContentManagerTestHelper;

  void SetReportingManagerForTesting(DlpReportingManager* manager);

  void SetWarnNotifierForTesting(
      std::unique_ptr<DlpWarnNotifier> warn_notifier);
  void ResetWarnNotifierForTesting();

  // Used to keep track of running screen shares.
  class ScreenShareInfo {
   public:
    ScreenShareInfo(
        const std::string& label,
        const content::DesktopMediaID& media_id,
        const std::u16string& application_title,
        base::OnceClosure stop_callback,
        content::MediaStreamUI::StateChangeCallback state_change_callback,
        content::MediaStreamUI::SourceCallback source_callback);
    ~ScreenShareInfo();

    bool operator==(const ScreenShareInfo& other) const;
    bool operator!=(const ScreenShareInfo& other) const;

    const content::DesktopMediaID& GetMediaId() const;
    const std::string& GetLabel() const;
    const std::u16string& GetApplicationTitle() const;
    bool IsRunning() const;
    base::WeakPtr<content::WebContents> GetWebContents() const;

    // Pauses a running screen share.
    // No-op if the screen share is already paused.
    void Pause();
    // Resumes a paused screen share.
    // No-op if the screen share is already running.
    void Resume();
    // Stops the screen share. Can only be called once.
    void Stop();

    // If necessary, hides or shows the paused/resumed notification for this
    // screen share. The notification should be updated after changing the state
    // from running to paused, or paused to running.
    void MaybeUpdateNotifications();

    // If shown, hides both the paused and resumed notification for this screen
    // share.
    void HideNotifications();

    base::WeakPtr<ScreenShareInfo> GetWeakPtr();

   private:
    enum class NotificationState {
      kNotShowingNotification,
      kShowingPausedNotification,
      kShowingResumedNotification
    };
    enum class State { kRunning, kPaused, kStopped };
    // Shows (if |show| is true) or hides (if |show| is false) paused
    // notification for this screen share. Does nothing if the notification is
    // already in the required state.
    void UpdatePausedNotification(bool show);
    // Shows (if |show| is true) or hides (if |show| is false) resumed
    // notification for this screen share. Does nothing if the notification is
    // already in the required state.
    void UpdateResumedNotification(bool show);

    std::string label_;
    content::DesktopMediaID media_id_;
    // TODO(crbug.com/1264793): Don't cache the application name.
    std::u16string application_title_;
    base::OnceClosure stop_callback_;
    content::MediaStreamUI::StateChangeCallback state_change_callback_;
    content::MediaStreamUI::SourceCallback source_callback_;
    State state_ = State::kRunning;
    NotificationState notification_state_ =
        NotificationState::kNotShowingNotification;

    // Set only for tab shares.
    base::WeakPtr<content::WebContents> web_contents_;

    base::WeakPtrFactory<ScreenShareInfo> weak_factory_{this};
  };

  // Structure that relates a list of confidential contents to the
  // corresponding restriction level.
  struct ConfidentialContentsInfo {
    RestrictionLevelAndUrl restriction_info;
    DlpConfidentialContents confidential_contents;
  };

  DlpContentManager();
  ~DlpContentManager() override;

  // Reports events of type DlpPolicyEvent::Mode::WARN_PROCEED to
  // `reporting_manager`.
  static void ReportWarningProceededEvent(
      const GURL& url,
      DlpRulesManager::Restriction restriction,
      DlpReportingManager* reporting_manager);

  // Helper method to create a callback with ReportWarningProceededEvent
  // function.
  static bool MaybeReportWarningProceededEvent(
      GURL url,
      DlpRulesManager::Restriction restriction,
      DlpReportingManager* reporting_manager,
      bool should_proceed);

  // Retrieves WebContents from |media_id| for tab shares. Otherwise returns
  // nullptr.
  static content::WebContents* GetWebContentsFromMediaId(
      const content::DesktopMediaID& media_id);

  // Initializing to be called separately to make testing possible.
  virtual void Init();

  // DlpContentObserver overrides:
  void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Called when tab was probably moved, but without change of the visibility.
  virtual void TabLocationMaybeChanged(content::WebContents* web_contents) = 0;

  // Helper to remove |web_contents| from the confidential set.
  virtual void RemoveFromConfidential(content::WebContents* web_contents);

  // Returns which level and url of printing restriction is currently enforced
  // for |web_contents|.
  RestrictionLevelAndUrl GetPrintingRestrictionInfo(
      content::WebContents* web_contents) const;

  // Returns confidential info for screen share of a single |web_contents|.
  ConfidentialContentsInfo GetScreenShareConfidentialContentsInfoForWebContents(
      content::WebContents* web_contents) const;

  // Applies retrieved restrictions in |info| to screens share attempt from
  // app |application_title| and calls the |callback| with a result.
  void ProcessScreenShareRestriction(const std::u16string& application_title,
                                     ConfidentialContentsInfo info,
                                     OnDlpRestrictionCheckedCallback callback);

  // Returns which level, url, and information about visible confidential
  // contents of screen share restriction that is currently enforced for
  // |media_id|. |web_contents| is not null for tab shares.
  virtual ConfidentialContentsInfo GetScreenShareConfidentialContentsInfo(
      const content::DesktopMediaID& media_id,
      content::WebContents* web_contents) const = 0;

  // Adds screen share to be tracked in |running_screen_shares_|.
  // Callbacks are used to control the screen share state in case it should be
  // paused, resumed or completely stopped by DLP.
  void AddScreenShare(
      const std::string& label,
      const content::DesktopMediaID& media_id,
      const std::u16string& application_title,
      base::RepeatingClosure stop_callback,
      content::MediaStreamUI::StateChangeCallback state_change_callback,
      content::MediaStreamUI::SourceCallback source_callback);

  // Removes screen share from |running_screen_shares_|.
  void RemoveScreenShare(const std::string& label,
                         const content::DesktopMediaID& media_id);

  // Checks and stops the running screen shares if restricted content appeared
  // in the corresponding areas.
  void CheckRunningScreenShares();

  // Called back from Screen Share warning dialogs that are shown during the
  // screen share. Passes along the user's response, reflected in the value of
  // |should_proceed| along to |callback| which handles continuing or cancelling
  // the action based on this response. In case that |should_proceed| is true,
  // also saves the |confidential_contents| that were allowed to be shared by
  // the user to avoid future warnings.
  void OnDlpScreenShareWarnDialogReply(
      const ConfidentialContentsInfo& info,
      base::WeakPtr<ScreenShareInfo> screen_share,
      bool should_proceed);

  // Called back from warning dialogs. Passes along the user's response,
  // reflected in the value of |should_proceed| along to |callback| which
  // handles continuing or cancelling the action based on this response. In case
  // that |should_proceed| is true, also saves the |confidential_contents| that
  // were allowed by the user for |restriction| to avoid future warnings.
  void OnDlpWarnDialogReply(
      const DlpConfidentialContents& confidential_contents,
      DlpRulesManager::Restriction restriction,
      OnDlpRestrictionCheckedCallback callback,
      bool should_proceed);

  // Reports events if required by the |restriction_info| and
  // `reporting_manager` is configured.
  void MaybeReportEvent(const RestrictionLevelAndUrl& restriction_info,
                        DlpRulesManager::Restriction restriction);

  // Reports warning events if `reporting_manager` is configured.
  void ReportWarningEvent(const GURL& url,
                          DlpRulesManager::Restriction restriction);

  // Removes all elemxents of |contents| that the user has recently already
  // acknowledged the warning for.
  void RemoveAllowedContents(DlpConfidentialContents& contents,
                             DlpRulesManager::Restriction restriction);

  // Notifies observers if the restrictions they are listening to changed.
  void NotifyOnConfidentialityChanged(
      const DlpContentRestrictionSet& old_restriction_set,
      const DlpContentRestrictionSet& new_restriction_set,
      content::WebContents* web_contents);

  // Map from currently known confidential WebContents to the restrictions.
  base::flat_map<content::WebContents*, DlpContentRestrictionSet>
      confidential_web_contents_;

  // Keeps track of the contents for which the user allowed the action after
  // being shown a warning for each type of restriction.
  // TODO(crbug.com/1264803): Change to DlpConfidentialContentsCache
  DlpConfidentialContentsCache user_allowed_contents_cache_;

  // List of the currently running screen shares.
  std::vector<std::unique_ptr<ScreenShareInfo>> running_screen_shares_;

  raw_ptr<DlpReportingManager> reporting_manager_{nullptr};

  std::unique_ptr<DlpWarnNotifier> warn_notifier_;

  // One ObserverList per restriction.
  std::array<base::ObserverList<DlpContentManagerObserver>,
             DlpContentRestriction::kMaxValue + 1>
      observer_lists_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_
