// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/element_area.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/payment_request.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_tracker.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace base {
class TickClock;
}  // namespace base

namespace autofill_assistant {
class ControllerTest;

// Autofill assistant controller controls autofill assistant action detection,
// display, execution and so on. The instance of this object self deletes when
// the web contents is being destroyed.
class Controller : public ScriptExecutorDelegate,
                   public UiDelegate,
                   public ScriptTracker::Listener,
                   private content::WebContentsObserver {
 public:
  // |web_contents|, |client| and |tick_clock| must remain valid for the
  // lifetime of the instance.
  Controller(content::WebContents* web_contents,
             Client* client,
             const base::TickClock* tick_clock);
  ~Controller() override;

  // Returns true if the controller is in a state where UI is necessary.
  bool NeedsUI() const;

  // Called when autofill assistant can start executing scripts.
  void Start(const GURL& initial_url,
             const std::map<std::string, std::string>& parameters);

  // Initiates a clean shutdown.
  //
  // This function returns false when it needs more time to properly shut down
  // the script tracker. In that case, the controller is responsible for calling
  // Client::Shutdown at the right time for the given reason.
  //
  // A caller is expected to try again later when this function returns false. A
  // return value of true means that the scrip tracker can safely be destroyed.
  //
  // TODO(crbug.com/806868): Instead of this safety net, the proper fix is to
  // switch to weak pointers everywhere so that dangling callbacks are not an
  // issue.
  bool Terminate(Metrics::DropOutReason reason);

  // Overrides ScriptExecutorDelegate:
  const GURL& GetCurrentURL() override;
  Service* GetService() override;
  UiController* GetUiController() override;
  WebController* GetWebController() override;
  ClientMemory* GetClientMemory() override;
  const std::map<std::string, std::string>& GetParameters() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  content::WebContents* GetWebContents() override;
  void SetTouchableElementArea(const ElementAreaProto& area) override;
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() const override;
  void SetDetails(std::unique_ptr<Details> details) override;
  void SetInfoBox(const InfoBox& info_box) override;
  void ClearInfoBox() override;
  void SetProgress(int progress) override;
  void SetProgressVisible(bool visible) override;
  void SetChips(std::unique_ptr<std::vector<Chip>> chips) override;

  // Stops the controller with |reason| and destroys this. The current status
  // message must contain the error message.
  void StopAndShutdown(Metrics::DropOutReason reason);
  void EnterState(AutofillAssistantState state) override;
  bool IsCookieExperimentEnabled() const;
  void SetPaymentRequestOptions(
      std::unique_ptr<PaymentRequestOptions> options) override;
  void CancelPaymentRequest() override;

  // Overrides autofill_assistant::UiDelegate:
  AutofillAssistantState GetState() override;
  void UpdateTouchableArea() override;
  void OnUserInteractionInsideTouchableArea() override;
  const Details* GetDetails() const override;
  const InfoBox* GetInfoBox() const override;
  int GetProgress() const override;
  bool GetProgressVisible() const override;
  const std::vector<Chip>& GetSuggestions() const override;
  void SelectSuggestion(int index) override;
  const std::vector<Chip>& GetActions() const override;
  void SelectAction(int index) override;
  std::string GetDebugContext() override;
  const PaymentRequestOptions* GetPaymentRequestOptions() const override;
  void SetPaymentInformation(
      std::unique_ptr<PaymentInformation> payment_information) override;
  void GetTouchableArea(std::vector<RectF>* area) const override;
  void OnFatalError(const std::string& error_message,
                    Metrics::DropOutReason reason) override;

 private:
  friend ControllerTest;

  void SetWebControllerAndServiceForTest(
      std::unique_ptr<WebController> web_controller,
      std::unique_ptr<Service> service);

  void GetOrCheckScripts();
  void OnGetScripts(const GURL& url, bool result, const std::string& response);

  // Execute |script_path| and, if execution succeeds, enter |end_state| and
  // call |on_success|.
  void ExecuteScript(const std::string& script_path,
                     AutofillAssistantState end_state);
  void OnScriptExecuted(const std::string& script_path,
                        AutofillAssistantState end_state,
                        const ScriptExecutor::Result& result);

  // Check script preconditions every few seconds for a certain number of times.
  // If checks are already running, StartPeriodicScriptChecks resets the count.
  //
  // TODO(crbug.com/806868): Find a better solution. This is a brute-force
  // solution that reacts slowly to changes.
  void StartPeriodicScriptChecks();
  void StopPeriodicScriptChecks();
  void OnPeriodicScriptCheck();

  // Runs autostart scripts from |runnable_scripts|, if the conditions are
  // right. Returns true if a script was auto-started.
  bool MaybeAutostartScript(const std::vector<ScriptHandle>& runnable_scripts);

  void DisableAutostart();

  // Autofill Assistant cookie logic.
  //
  // On startup of the controller we set a cookie. If a cookie already existed
  // for the intial URL, we show a warning that the website has already been
  // visited and could contain old data. The cookie is cleared (or expires) when
  // a script terminated with a Stop action.
  void OnGetCookie(bool has_cookie);
  void OnSetCookie(bool result);
  void FinishStart();
  void MaybeSetInitialDetails();

  // Called when a script is selected.
  void OnScriptSelected(const std::string& script_path);

  // Overrides ScriptTracker::Listener:
  void OnNoRunnableScripts() override;
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override;

  // Overrides content::WebContentsObserver:
  void DidAttachInterstitialPage() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;

  void OnTouchableAreaChanged(const std::vector<RectF>& areas);

  void SelectChip(std::vector<Chip>* chips, int chip_index);

  ElementArea* touchable_element_area();
  ScriptTracker* script_tracker();

  Client* const client_;
  const base::TickClock* const tick_clock_;

  std::unique_ptr<UiController> noop_ui_controller_;

  // Lazily instantiate in GetWebController().
  std::unique_ptr<WebController> web_controller_;

  // Lazily instantiate in GetService().
  std::unique_ptr<Service> service_;
  std::map<std::string, std::string> parameters_;

  // Lazily instantiate in GetClientMemory().
  std::unique_ptr<ClientMemory> memory_;

  AutofillAssistantState state_ = AutofillAssistantState::INACTIVE;

  // The URL passed to Start(). Used only as long as there's no committed URL.
  GURL initial_url_;

  // Domain of the last URL the controller requested scripts from.
  std::string script_domain_;
  bool allow_autostart_ = true;

  // Whether a task for periodic checks is scheduled.
  bool periodic_script_check_scheduled_ = false;

  // Number of remaining periodic checks.
  int periodic_script_check_count_ = 0;

  // Run this script if no scripts become autostartable after
  // absolute_autostart_timeout.
  //
  // Ignored unless |allow_autostart_| is true.
  std::string autostart_timeout_script_path_;

  // How long to wait for an autostartable script before failing.
  base::TimeDelta autostart_timeout_;

  // Ticks at which we'll have reached |autostart_timeout_|.
  base::TimeTicks absolute_autostart_timeout_;

  // Area of the screen that corresponds to the current set of touchable
  // elements.
  // Lazily instantiate in touchable_element_area().
  std::unique_ptr<ElementArea> touchable_element_area_;

  // Current status message, may be empty.
  std::string status_message_;

  // Current details, may be null.
  std::unique_ptr<Details> details_;

  // Current info box, may be null.
  std::unique_ptr<InfoBox> info_box_;

  // Current progress.
  int progress_ = 0;

  // Current visibility of the progress bar. It is initially visible.
  bool progress_visible_ = true;

  // Current set of suggestions. May be null, but never empty.
  std::unique_ptr<std::vector<Chip>> suggestions_;

  // Current set of actions. May be null, but never empty.
  std::unique_ptr<std::vector<Chip>> actions_;

  // Flag indicates whether it is ready to fetch and execute scripts.
  bool started_ = false;

  // A reason passed previously to Terminate(). SAFETY_NET_TERMINATE is a
  // placeholder.
  Metrics::DropOutReason terminate_reason_ = Metrics::SAFETY_NET_TERMINATE;

  // True once UiController::WillShutdown has been called.
  bool will_shutdown_ = false;

  std::unique_ptr<PaymentRequestOptions> payment_request_options_;

  // Tracks scripts and script execution. It's kept at the end, as it tend to
  // depend on everything the controller support, through script and script
  // actions.
  // Lazily instantiate in script_tracker().
  std::unique_ptr<ScriptTracker> script_tracker_;

  base::WeakPtrFactory<Controller> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
