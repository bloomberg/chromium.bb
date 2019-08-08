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
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/element_area.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/payment_request.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_tracker.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/trigger_context.h"
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
  void Start(const GURL& deeplink_url,
             std::unique_ptr<TriggerContext> trigger_context);

  // Lets the controller know it's about to be deleted. This is normally called
  // from the client.
  void WillShutdown(Metrics::DropOutReason reason);

  // Overrides ScriptExecutorDelegate:
  const ClientSettings& GetSettings() override;
  const GURL& GetCurrentURL() override;
  const GURL& GetDeeplinkURL() override;
  Service* GetService() override;
  UiController* GetUiController() override;
  WebController* GetWebController() override;
  ClientMemory* GetClientMemory() override;
  const TriggerContext* GetTriggerContext() override;
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
  void SetResizeViewport(bool resize_viewport) override;
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  bool SetForm(std::unique_ptr<FormProto> form,
               base::RepeatingCallback<void(const FormProto::Result*)> callback)
      override;
  bool IsNavigatingToNewDocument() override;
  bool HasNavigationError() override;
  void AddListener(ScriptExecutorDelegate::Listener* listener) override;
  void RemoveListener(ScriptExecutorDelegate::Listener* listener) override;

  void EnterState(AutofillAssistantState state) override;
  bool IsCookieExperimentEnabled() const;
  void SetPaymentRequestOptions(
      std::unique_ptr<PaymentRequestOptions> options) override;

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
  const PaymentInformation* GetPaymentRequestInformation() const override;
  void SetShippingAddress(
      std::unique_ptr<autofill::AutofillProfile> address) override;
  void SetBillingAddress(
      std::unique_ptr<autofill::AutofillProfile> address) override;
  void SetContactInfo(std::string name,
                      std::string phone,
                      std::string email) override;
  void SetCreditCard(std::unique_ptr<autofill::CreditCard> card) override;
  void SetTermsAndConditions(
      TermsAndConditionsState terms_and_conditions) override;
  void GetTouchableArea(std::vector<RectF>* area) const override;
  void GetVisualViewport(RectF* visual_viewport) const override;
  void OnFatalError(const std::string& error_message,
                    Metrics::DropOutReason reason) override;
  bool GetResizeViewport() override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() override;
  void GetOverlayColors(OverlayColors* colors) const override;
  const FormProto* GetForm() const override;
  void SetCounterValue(int input_index, int counter_index, int value) override;
  void SetChoiceSelected(int input_index,
                         int choice_index,
                         bool selected) override;

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
  void InitFromParameters();

  // Called when a script is selected.
  void OnScriptSelected(const std::string& script_path);

  void UpdatePaymentRequestActions();
  void OnPaymentRequestContinueButtonClicked();

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
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;

  void OnTouchableAreaChanged(const RectF& visual_viewport,
                              const std::vector<RectF>& areas);

  void SelectChip(std::vector<Chip>* chips, int chip_index);
  void SetOverlayColors(std::unique_ptr<OverlayColors> colors);
  void ReportNavigationStateChanged();

  // Clear out visible state and enter the stopped state.
  void EnterStoppedState();

  ElementArea* touchable_element_area();
  ScriptTracker* script_tracker();

  ClientSettings settings_;
  Client* const client_;
  const base::TickClock* const tick_clock_;

  std::unique_ptr<UiController> noop_ui_controller_;

  // Lazily instantiate in GetWebController().
  std::unique_ptr<WebController> web_controller_;

  // Lazily instantiate in GetService().
  std::unique_ptr<Service> service_;
  std::unique_ptr<TriggerContext> trigger_context_;

  // Lazily instantiate in GetClientMemory().
  std::unique_ptr<ClientMemory> memory_;

  AutofillAssistantState state_ = AutofillAssistantState::INACTIVE;

  // The URL passed to Start(). Used only as long as there's no committed URL.
  // Note that this is the deeplink passed by a caller and reported to the
  // backend in an initial get action request.
  GURL deeplink_url_;

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

  // Whether the viewport should be resized.
  bool resize_viewport_ = false;

  // Current peek mode.
  ConfigureBottomSheetProto::PeekMode peek_mode_ =
      ConfigureBottomSheetProto::HANDLE;

  std::unique_ptr<OverlayColors> overlay_colors_;

  // Flag indicates whether it is ready to fetch and execute scripts.
  bool started_ = false;

  // True once UiController::WillShutdown has been called.
  bool will_shutdown_ = false;

  std::unique_ptr<PaymentRequestOptions> payment_request_options_;
  std::unique_ptr<PaymentInformation> payment_request_info_;

  std::unique_ptr<FormProto> form_;
  std::unique_ptr<FormProto::Result> form_result_;
  base::RepeatingCallback<void(const FormProto::Result*)> form_callback_ =
      base::DoNothing();

  // Value for ScriptExecutorDelegate::IsNavigatingToNewDocument()
  bool navigating_to_new_document_ = false;

  // Value for ScriptExecutorDelegate::HasNavigationError()
  bool navigation_error_ = false;
  std::vector<ScriptExecutorDelegate::Listener*> listeners_;

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
