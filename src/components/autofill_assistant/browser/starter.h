// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/starter_heuristic.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "components/autofill_assistant/browser/startup_util.h"
#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Starts autofill-assistant flows. Uses a platform delegate to show UI and
// access platform-dependent features.
class Starter : public content::WebContentsObserver {
 public:
  explicit Starter(content::WebContents* web_contents,
                   StarterPlatformDelegate* platform_delegate,
                   ukm::UkmRecorder* ukm_recorder,
                   base::WeakPtr<RuntimeManager> runtime_manager,
                   const base::TickClock* tick_clock);
  ~Starter() override;
  Starter(const Starter&) = delete;
  Starter& operator=(const Starter&) = delete;

  // Entry-point for all non-direct-action flows. This method will perform the
  // overall startup procedure, which is roughly as follows:
  //  - Check parameters/features/settings and determine startup mode
  //  - Install feature module if necessary
  //  - Run and wait for trigger script to finish if necessary
  //  - Show onboarding if necessary
  //  - Request the platform_delegate to start the regular script.
  // TODO(mcarlen): client startup should also be handled here, rather than in
  // the platform_delegate.
  //
  // Only one call to |Start| can be processed at any time. If this method is
  // called before the previous call has finished, the previous call is
  // cancelled.
  void Start(std::unique_ptr<TriggerContext> trigger_context);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // Invoked when the tab interactability has changed.
  void OnTabInteractabilityChanged(bool is_interactable);

  // Re-check settings. This may cancel ongoing startup requests if the required
  // settings are no longer enabled.
  void CheckSettings();

  // Records the invalidation of platform-specific depencendies. For example:
  // When the activity is changed on Android.
  void OnDependenciesInvalidated();

 private:
  friend class StarterTest;

  // Starts a flow for |url| if possible. Will fail (do nothing) if the feature
  // is disabled or if there is already a pending startup.
  void MaybeStartImplicitlyForUrl(const GURL& url,
                                  const ukm::SourceId source_id);

  // Cancels the currently pending startup request, if any. If a trigger script
  // is currently running, this will record |state| as the reason for stopping.
  // This will also hide any currently shown UI (such as a trigger script or the
  // onboarding).
  void CancelPendingStartup(
      absl::optional<Metrics::TriggerScriptFinishedState> state);

  // Installs the feature module if necessary, otherwise directly invokes
  // |OnFeatureModuleInstalled|.
  void MaybeInstallFeatureModule(StartupUtil::StartupMode startup_mode);

  // Stops the startup if the installation failed. Otherwise, proceeds with the
  // next step of the startup process.
  void OnFeatureModuleInstalled(StartupUtil::StartupMode startup_mode,
                                Metrics::FeatureModuleInstallation result);

  // Starts a trigger script and waits for it to finish in
  // |OnTriggerScriptFinished|.
  void StartTriggerScript();

  // Stops the startup if the trigger script failed or was user-cancelled.
  // Otherwise, proceeds with the start of the regular script.
  void OnTriggerScriptFinished(
      Metrics::TriggerScriptFinishedState state,
      std::unique_ptr<TriggerContext> trigger_context,
      absl::optional<TriggerScriptProto> trigger_script);

  // Shows the onboarding if necessary, otherwise directly invokes
  // |OnOnboardingFinished|.
  void MaybeShowOnboarding(
      absl::optional<TriggerScriptProto> trigger_script = absl::nullopt);

  // Starts the regular script if onboarding was accepted. Stops the startup
  // process if onboarding was rejected.
  void OnOnboardingFinished(absl::optional<TriggerScriptProto> trigger_script,
                            bool shown,
                            OnboardingResult result);

  // Called at the end of each |Start| invocation.
  void OnStartDone(
      bool start_regular_script,
      absl::optional<TriggerScriptProto> trigger_script = absl::nullopt);

  // Called when the heuristic result for |url| is available.
  void OnHeuristicMatch(const GURL& url,
                        const ukm::SourceId source_id,
                        const base::flat_set<std::string>& intents);

  // Returns whether there is a currently pending call to |Start| or not.
  bool IsStartupPending() const;

  // Deletes the trigger script coordinator.
  void DeleteTriggerScriptCoordinator();

  // Records metrics when the dependencies get invalidated.
  void RecordDependenciesInvalidated() const;

  // Returns a pointer to the currently pending trigger context, or nullptr.
  // Use this method instead of directly accessing |pending_trigger_context_| in
  // cases where the context could be temporarily owned by
  // |trigger_script_coordinator_|.
  TriggerContext* GetPendingTriggerContext() const;

  // The UKM source id to use for UKM metrics.
  ukm::SourceId current_ukm_source_id_ = ukm::kInvalidSourceId;

  // Pointer to the global cache of trigger script requests that failed (one
  // entry per organization-identifying domain), along with the time of entry.
  // This is used to limit network traffic incurred for in-chrome triggering
  // only. This cache does not affect explicit startup requests.
  //
  // This cache is shared across all tabs. It is size-limited and entries only
  // last for a limited amount of time before they go stale. Made available in
  // the header for easier unit-testing.
  raw_ptr<base::HashingLRUCache<std::string, base::TimeTicks>>
      cached_failed_trigger_script_fetches_;

  // The list of organization-identifying domains that a user has temporarily
  // opted out of for receiving implicit autofill-assistant prompts, along
  // with the time of entry.
  //
  // This is a per-tab cache. This cache does not affect explicit startup
  // requests. The cache is size-limited and entries only last for a limited
  // amount of time before they go stale.
  base::HashingLRUCache<std::string, base::TimeTicks> user_denylisted_domains_;

  // Debug parameters for in-CCT and in-Tab trigger scenarios. This is populated
  // from the command line and intended only for debugging and testing.
  ImplicitTriggeringDebugParametersProto implicit_triggering_debug_parameters_;

  bool waiting_for_onboarding_ = false;
  bool waiting_for_deeplink_navigation_ = false;
  bool is_custom_tab_ = false;
  const raw_ptr<StarterPlatformDelegate> platform_delegate_;
  raw_ptr<ukm::UkmRecorder> ukm_recorder_ = nullptr;
  base::WeakPtr<RuntimeManager> runtime_manager_;
  bool fetch_trigger_scripts_on_navigation_ = false;
  std::unique_ptr<TriggerContext> pending_trigger_context_;
  std::unique_ptr<TriggerScriptCoordinator> trigger_script_coordinator_;
  const scoped_refptr<StarterHeuristic> starter_heuristic_;
  raw_ptr<const base::TickClock> tick_clock_;
  base::WeakPtrFactory<Starter> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_H_
