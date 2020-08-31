// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-forward.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

class TabStripModel;

namespace content {
class RenderProcessHost;
class WebContents;
}  // namespace content

namespace resource_coordinator {

class UsageClock;
class TabLifecycleObserver;

// Time during which backgrounded tabs are protected from urgent discarding
// (not on ChromeOS).
static constexpr base::TimeDelta kBackgroundUrgentProtectionTime =
    base::TimeDelta::FromMinutes(10);

// Time during which a tab cannot be discarded after having played audio.
static constexpr base::TimeDelta kTabAudioProtectionTime =
    base::TimeDelta::FromMinutes(1);

class TabLifecycleUnitExternalImpl;

// Represents a tab.
class TabLifecycleUnitSource::TabLifecycleUnit
    : public LifecycleUnitBase,
      public content::WebContentsObserver {
 public:
  // |observers| is a list of observers to notify when the discarded state or
  // the auto-discardable state of this tab changes. It can be modified outside
  // of this TabLifecycleUnit, but only on the sequence on which this
  // constructor is invoked. |usage_clock| is a clock that measures Chrome usage
  // time. |web_contents| and |tab_strip_model| are the WebContents and
  // TabStripModel associated with this tab. The |source| is optional and may be
  // nullptr.
  TabLifecycleUnit(
      TabLifecycleUnitSource* source,
      base::ObserverList<TabLifecycleObserver>::Unchecked* observers,
      UsageClock* usage_clock,
      content::WebContents* web_contents,
      TabStripModel* tab_strip_model);
  ~TabLifecycleUnit() override;

  // Sets the TabStripModel associated with this tab. The source that created
  // this TabLifecycleUnit is responsible for calling this when the tab is
  // removed from a TabStripModel or inserted into a new TabStripModel.
  void SetTabStripModel(TabStripModel* tab_strip_model);

  // Sets the WebContents associated with this tab. The source that created this
  // TabLifecycleUnit is responsible for calling this when the tab's WebContents
  // changes (e.g. when the tab is discarded or when prerendered or distilled
  // content is displayed).
  void SetWebContents(content::WebContents* web_contents);

  // Invoked when the tab gains or loses focus.
  void SetFocused(bool focused);

  // Sets the "recently audible" state of this tab. A tab is "recently audible"
  // if a speaker icon is displayed next to it in the tab strip. The source that
  // created this TabLifecycleUnit is responsible for calling this when the
  // "recently audible" state of the tab changes.
  void SetRecentlyAudible(bool recently_audible);

  // Set the initial state of this lifecycle unit with some data coming from the
  // performance_manager Graph.
  void SetInitialStateFromPageNodeData(
      performance_manager::mojom::InterventionPolicy origin_trial_policy,
      bool is_holding_weblock,
      bool is_holding_indexeddb_lock);

  // Updates the tab's lifecycle state when changed outside the tab
  // lifecycle unit.
  void UpdateLifecycleState(performance_manager::mojom::LifecycleState state);

  // Updates the tab's origin trial freeze policy.
  void UpdateOriginTrialFreezePolicy(
      performance_manager::mojom::InterventionPolicy policy);

  // Setters for the WebLock and IndexedDB lock usage properties.
  void SetIsHoldingWebLock(bool is_holding_weblock);
  void SetIsHoldingIndexedDBLock(bool is_holding_indexeddb_lock);

  // LifecycleUnit:
  TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() override;
  base::string16 GetTitle() const override;
  base::TimeTicks GetLastFocusedTime() const override;
  base::ProcessHandle GetProcessHandle() const override;
  SortKey GetSortKey() const override;
  content::Visibility GetVisibility() const override;
  LifecycleUnitLoadingState GetLoadingState() const override;
  bool Load() override;
  int GetEstimatedMemoryFreedOnDiscardKB() const override;
  bool CanFreeze(DecisionDetails* decision_details) const override;
  bool CanDiscard(LifecycleUnitDiscardReason reason,
                  DecisionDetails* decision_details) const override;
  LifecycleUnitDiscardReason GetDiscardReason() const override;
  bool Freeze() override;
  bool Unfreeze() override;
  bool Discard(LifecycleUnitDiscardReason discard_reason) override;
  ukm::SourceId GetUkmSourceId() const override;

  // Implementations of some functions from TabLifecycleUnitExternal. These are
  // actually called by an instance of TabLifecycleUnitExternalImpl.
  bool IsAutoDiscardable() const;
  void SetAutoDiscardable(bool auto_discardable);

  bool IsHoldingWebLockForTesting() { return is_holding_weblock_; }

 protected:
  friend class TabManagerTest;

  // TabLifecycleUnitSource needs to update the state when a external lifecycle
  // state change is observed.
  friend class TabLifecycleUnitSource;

 private:
  // Same as GetSource, but cast to the most derived type.
  TabLifecycleUnitSource* GetTabSource() const;

  // Updates |decision_details| based on media usage by the tab.
  void CheckMediaUsage(DecisionDetails* decision_details) const;

  // For non-urgent discarding, sends a request for freezing to occur prior to
  // discarding the tab.
  void RequestFreezeForDiscard(LifecycleUnitDiscardReason reason);

  // Finishes a tab discard, invoked by Discard().
  void FinishDiscard(LifecycleUnitDiscardReason discard_reason);

  // Returns the RenderProcessHost associated with this tab.
  content::RenderProcessHost* GetRenderProcessHost() const;

  // LifecycleUnitBase:
  void OnLifecycleUnitStateChanged(
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason) override;

  // content::WebContentsObserver:
  void DidStartLoading() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Updates |decision_details| based on device usage by the tab (USB or
  // Bluetooth).
  void CheckDeviceUsage(DecisionDetails* decision_details) const;

  // Updates |decision_details| based on freezing origin trial opt-in/opt-out
  // for the tab.
  void CheckFreezingOriginTrial(DecisionDetails* decision_details) const;

  // Updates |decision_details| based on the global intervention policy database
  // freezing data for the tab.
  void CheckFreezingInterventionPolicyDatabase(
      DecisionDetails* decision_details) const;

  // List of observers to notify when the discarded state or the auto-
  // discardable state of this tab changes.
  base::ObserverList<TabLifecycleObserver>::Unchecked* observers_;

  // TabStripModel to which this tab belongs.
  TabStripModel* tab_strip_model_;

  // Last time at which this tab was focused, or TimeTicks::Max() if it is
  // currently focused. For tabs that aren't currently focused this is
  // initialized using WebContents::GetLastActiveTime, which causes use times
  // from previous browsing sessions to persist across session restore
  // events.
  // TODO(chrisha): Migrate |last_active_time| to actually track focus time,
  // instead of the time that focus was lost. This is a more meaninful number
  // for all of the clients of |last_active_time|.
  base::TimeTicks last_focused_time_;

  // When this is false, CanDiscard() always returns false.
  bool auto_discardable_ = true;

  // The freeze policy set via origin trial. Initial value is kDefault to avoid
  // affecting CanFreeze() before the policy is set for the first time.
  performance_manager::mojom::InterventionPolicy origin_trial_freeze_policy_ =
      performance_manager::mojom::InterventionPolicy::kDefault;

  // Maintains the most recent LifecycleUnitDiscardReason that was passed into
  // Discard().
  LifecycleUnitDiscardReason discard_reason_ =
      LifecycleUnitDiscardReason::EXTERNAL;

  // TimeTicks::Max() if the tab is currently "recently audible", null
  // TimeTicks() if the tab was never "recently audible", last time at which the
  // tab was "recently audible" otherwise.
  base::TimeTicks recently_audible_time_;

  // Indicates if at least one of the frames of this tab is currently holding
  // at least one WebLock.
  bool is_holding_weblock_ = false;

  // Indicates if at least one of the frames of this tab is currently holding
  // at least one IndexedDB Lock.
  bool is_holding_indexeddb_lock_ = false;

  std::unique_ptr<TabLifecycleUnitExternalImpl> external_impl_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnit);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
