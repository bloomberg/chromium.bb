// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/browser/background_sync/background_sync.pb.h"
#include "content/browser/background_sync/background_sync_proxy.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/browser/background_sync_registration.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
namespace mojom {
enum class PermissionStatus;
}  // namespace mojom
}  // namespace blink

namespace content {

class BackgroundSyncNetworkObserver;
class ServiceWorkerContextWrapper;

// BackgroundSyncManager manages and stores the set of background sync
// registrations across all registered service workers for a profile.
// Registrations are stored along with their associated Service Worker
// registration in ServiceWorkerStorage. If the ServiceWorker is unregistered,
// the sync registrations are removed. This class must be run on the IO
// thread. The asynchronous methods are executed sequentially.
class CONTENT_EXPORT BackgroundSyncManager
    : public ServiceWorkerContextCoreObserver {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;
  using StatusCallback = base::OnceCallback<void(BackgroundSyncStatus)>;
  using StatusAndRegistrationCallback =
      base::OnceCallback<void(BackgroundSyncStatus,
                              std::unique_ptr<BackgroundSyncRegistration>)>;
  using StatusAndRegistrationsCallback = base::OnceCallback<void(
      BackgroundSyncStatus,
      std::vector<std::unique_ptr<BackgroundSyncRegistration>>)>;
  using BackgroundSyncEventKeepAlive =
      BackgroundSyncController::BackgroundSyncEventKeepAlive;

  static std::unique_ptr<BackgroundSyncManager> Create(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context);
  ~BackgroundSyncManager() override;

  // Stores the given background sync registration and adds it to the scheduling
  // queue. It will overwrite an existing registration with the same tag unless
  // they're identical (save for the id). Calls |callback| with
  // BACKGROUND_SYNC_STATUS_OK and the accepted registration on success.
  // The accepted registration will have a unique id. It may also have altered
  // parameters if the user or UA chose different parameters than those
  // supplied.
  void Register(int64_t sw_registration_id,
                blink::mojom::SyncRegistrationOptions options,
                StatusAndRegistrationCallback callback);

  // Removes the Periodic Background Sync registration identified by |tag| for
  // the service worker identified by |sw_registration_id|. Calls |callback|
  // with BACKGROUND_SYNC_STATUS_OK on success.
  void UnregisterPeriodicSync(int64_t sw_registration_id,
                              const std::string& tag,
                              StatusCallback callback);

  // Called after the client has resolved its registration promise. At this
  // point it's safe to fire any pending registrations.
  void DidResolveRegistration(
      blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info);

  // Finds the one-shot Background Sync registrations associated with
  // |sw_registration_id|. Calls |callback| with BACKGROUND_SYNC_STATUS_OK on
  // success.
  void GetOneShotSyncRegistrations(int64_t sw_registration_id,
                                   StatusAndRegistrationsCallback callback);

  // Finds the periodic Background Sync registrations associated with
  // |sw_registration_id|. Calls |callback| with BACKGROUND_SYNC_STATUS_OK on
  // success.
  void GetPeriodicSyncRegistrations(int64_t sw_registration_id,
                                    StatusAndRegistrationsCallback callback);

  // ServiceWorkerContextCoreObserver overrides.
  void OnRegistrationDeleted(int64_t sw_registration_id,
                             const GURL& pattern) override;
  void OnStorageWiped() override;

  // Sets the max number of sync attempts after any pending operations have
  // completed.
  void SetMaxSyncAttemptsForTesting(int max_attempts);

  BackgroundSyncNetworkObserver* GetNetworkObserverForTesting() {
    return network_observer_.get();
  }

  void set_clock(base::Clock* clock) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    clock_ = clock;
  }

  // Called from DevTools
  void EmulateDispatchSyncEvent(
      const std::string& tag,
      scoped_refptr<ServiceWorkerVersion> active_version,
      bool last_chance,
      ServiceWorkerVersion::StatusCallback callback);

  // Called from DevTools to toggle service worker "offline" status
  void EmulateServiceWorkerOffline(int64_t service_worker_id, bool is_offline);

  // Scans the list of available events and fires those of type |sync_type| that
  // are ready to fire. For those that can't yet be fired, wakeup alarms are
  // set. Once all of this is done, invokes |callback|.
  void FireReadyEvents(
      blink::mojom::BackgroundSyncType sync_type,
      bool reschedule,
      base::OnceClosure callback,
      std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive = nullptr);

  // Gets the soonest delta after which the browser should be woken up to send
  // a Background Sync event. If set to max, the browser won't be woken up.
  // Only registrations of type |sync_type| are considered.
  // Browsers can have a hard limit on how often to wake themselves up to
  // process Periodic Background Sync registrations. We apply this limit if
  // |last_browser_wakeup_time| is not null.
  // This limit is only applied when calculating the soonest wake up delta to
  // wake up Chrome. It's not applied when calculating the time after which a
  // delayed task should be run to process Background Sync registrations.
  virtual base::TimeDelta GetSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType sync_type,
      base::Time last_browser_wakeup_time);

  // Browsers can have a hard limit on how often to wake themselves up to
  // process Periodic Background Sync registrations. If the browser can't be
  // woken up after |wakeup_delta| to do so, returns an updated delta after
  // which it's safe to wake the browser. This limit doesn't apply to retries.
  base::TimeDelta MaybeApplyBrowserWakeupCountLimit(
      base::TimeDelta wakeup_delta,
      base::Time last_browser_wakeup_time);

  // Each origin has a max_frequency decided by the browser. This picks the
  // correct starting point to add to |delay| to so that the resulting
  // |delay_until| for the |registration| ensures the minimum gap between
  // periodicsync events fired for the origin.
  // |delay| is only updated if |sync_type| is periodic.
  base::Time GetDelayUntilAfterApplyingMinGapForOrigin(
      blink::mojom::BackgroundSyncType sync_type,
      const url::Origin& origin,
      base::TimeDelta delay) const;

  base::Time GetSoonestPeriodicSyncEventTimeForOrigin(
      const url::Origin& origin) const;

  // Revive any pending periodic Background Sync registrations for |origin|.
  void RevivePeriodicSyncRegistrations(url::Origin origin);

 protected:
  BackgroundSyncManager(
      scoped_refptr<ServiceWorkerContextWrapper> context,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context);

  // Init must be called before any public member function. Only call it once.
  void Init();

  // The following methods are virtual for testing.
  virtual void StoreDataInBackend(
      int64_t sw_registration_id,
      const url::Origin& origin,
      const std::string& backend_key,
      const std::string& data,
      ServiceWorkerStorage::StatusCallback callback);
  virtual void GetDataFromBackend(
      const std::string& backend_key,
      ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback callback);
  virtual void DispatchSyncEvent(
      const std::string& tag,
      scoped_refptr<ServiceWorkerVersion> active_version,
      bool last_chance,
      ServiceWorkerVersion::StatusCallback callback);
  virtual void DispatchPeriodicSyncEvent(
      const std::string& tag,
      scoped_refptr<ServiceWorkerVersion> active_version,
      ServiceWorkerVersion::StatusCallback callback);
  virtual void ScheduleDelayedTask(blink::mojom::BackgroundSyncType sync_type,
                                   base::TimeDelta delay);
  virtual void HasMainFrameProviderHost(const url::Origin& origin,
                                        BoolCallback callback);

 private:
  friend class TestBackgroundSyncManager;
  friend class BackgroundSyncManagerTest;

  struct BackgroundSyncRegistrations {
    using RegistrationMap =
        std::map<std::pair<std::string, blink::mojom::BackgroundSyncType>,
                 BackgroundSyncRegistration>;

    BackgroundSyncRegistrations();
    BackgroundSyncRegistrations(const BackgroundSyncRegistrations& other);
    ~BackgroundSyncRegistrations();

    RegistrationMap registration_map;
    url::Origin origin;
  };

  static const size_t kMaxTagLength = 10240;

  // Disable the manager. Already queued operations will abort once they start
  // to run (in their impl methods). Future operations will not queue.
  // The list of active registrations is cleared and the backend is also cleared
  // (if it's still functioning). The manager will reenable itself once it
  // receives the OnStorageWiped message or on browser restart.
  void DisableAndClearManager(base::OnceClosure callback);
  void DisableAndClearDidGetRegistrations(
      base::OnceClosure callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status);
  void DisableAndClearManagerClearedOne(base::OnceClosure barrier_closure,
                                        blink::ServiceWorkerStatusCode status);

  // Returns the existing registration or nullptr if it cannot be found.
  BackgroundSyncRegistration* LookupActiveRegistration(
      const blink::mojom::BackgroundSyncRegistrationInfo& registration_info);

  // Write all registrations for a given |sw_registration_id| to persistent
  // storage.
  void StoreRegistrations(int64_t sw_registration_id,
                          ServiceWorkerStorage::StatusCallback callback);

  // Removes the active registration if it is in the map.
  void RemoveActiveRegistration(
      const blink::mojom::BackgroundSyncRegistrationInfo& registration_info);

  void AddOrUpdateActiveRegistration(
      int64_t sw_registration_id,
      const url::Origin& origin,
      const BackgroundSyncRegistration& sync_registration);

  void InitImpl(base::OnceClosure callback);
  void InitDidGetControllerParameters(
      base::OnceClosure callback,
      std::unique_ptr<BackgroundSyncParameters> parameters);
  void InitDidGetDataFromBackend(
      base::OnceClosure callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status);

  void GetRegistrations(blink::mojom::BackgroundSyncType sync_type,
                        int64_t sw_registration_id,
                        StatusAndRegistrationsCallback callback);

  // Register callbacks
  void RegisterCheckIfHasMainFrame(
      int64_t sw_registration_id,
      blink::mojom::SyncRegistrationOptions options,
      StatusAndRegistrationCallback callback);
  void RegisterDidCheckIfMainFrame(
      int64_t sw_registration_id,
      blink::mojom::SyncRegistrationOptions options,
      StatusAndRegistrationCallback callback,
      bool has_main_frame_client);
  void RegisterImpl(int64_t sw_registration_id,
                    blink::mojom::SyncRegistrationOptions options,
                    StatusAndRegistrationCallback callback);
  void RegisterDidAskForPermission(
      int64_t sw_registration_id,
      blink::mojom::SyncRegistrationOptions options,
      StatusAndRegistrationCallback callback,
      std::pair<blink::mojom::PermissionStatus, blink::mojom::PermissionStatus>
          permission_statuses);
  void RegisterDidGetDelay(int64_t sw_registration_id,
                           BackgroundSyncRegistration new_registration,
                           StatusAndRegistrationCallback callback,
                           base::TimeDelta delay);
  void RegisterDidStore(int64_t sw_registration_id,
                        const BackgroundSyncRegistration& new_registration,
                        StatusAndRegistrationCallback callback,
                        blink::ServiceWorkerStatusCode status);
  void UnregisterPeriodicSyncImpl(int64_t sw_registration_id,
                                  const std::string& tag,
                                  StatusCallback callback);
  void UnregisterPeriodicSyncDidStore(StatusCallback callback,
                                      blink::ServiceWorkerStatusCode status);

  // DidResolveRegistration callbacks
  void DidResolveRegistrationImpl(
      blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
      CacheStorageSchedulerId id);
  void ResolveRegistrationDidCreateKeepAlive(
      CacheStorageSchedulerId id,
      std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive);

  // GetRegistrations callbacks
  void GetRegistrationsImpl(blink::mojom::BackgroundSyncType sync_type,
                            int64_t sw_registration_id,
                            StatusAndRegistrationsCallback callback);

  bool AreOptionConditionsMet();
  bool IsRegistrationReadyToFire(const BackgroundSyncRegistration& registration,
                                 int64_t service_worker_id);

  // Determines if the browser needs to be able to run in the background (e.g.,
  // to run a pending registration or verify that a firing registration
  // completed). If background processing is required it calls out to the
  // BackgroundSyncController to enable it.
  // Assumes that all registrations in the pending state are not currently ready
  // to fire. Therefore this should not be called directly and should only be
  // called by FireReadyEvents.
  void ScheduleDelayedProcessingOfRegistrations(
      blink::mojom::BackgroundSyncType sync_type);

  base::CancelableOnceClosure& get_delayed_task(
      blink::mojom::BackgroundSyncType sync_type);

  void ResetAndScheduleDelayedSyncTask(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta soonest_wakeup_delta);

  void FireReadyEventsImpl(
      blink::mojom::BackgroundSyncType sync_type,
      bool reschedule,
      base::OnceClosure callback,
      std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive);

  void FireReadyEventsDidFindRegistration(
      blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
      std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive,
      base::OnceClosure event_fired_callback,
      base::OnceClosure event_completed_callback,
      blink::ServiceWorkerStatusCode service_worker_status,
      scoped_refptr<ServiceWorkerRegistration> service_worker_registration);
  void FireReadyEventsAllEventsFiring(
      blink::mojom::BackgroundSyncType sync_type,
      bool reschedule,
      base::OnceClosure callback);

  // Called when a sync event has completed.
  void EventComplete(
      scoped_refptr<ServiceWorkerRegistration> service_worker_registration,
      blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
      std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive,
      base::OnceClosure callback,
      blink::ServiceWorkerStatusCode status_code);
  void EventCompleteImpl(
      blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
      std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive,
      blink::ServiceWorkerStatusCode status_code,
      const url::Origin& origin,
      base::OnceClosure callback);
  void EventCompleteDidGetDelay(
      blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
      blink::ServiceWorkerStatusCode status_code,
      const url::Origin& origin,
      base::OnceClosure callback,
      base::TimeDelta delay);
  void EventCompleteDidStore(blink::mojom::BackgroundSyncType sync_type,
                             int64_t service_worker_id,
                             base::OnceClosure callback,
                             blink::ServiceWorkerStatusCode status_code);

  // Called when all sync events have completed.
  static void OnAllSyncEventsCompleted(
      blink::mojom::BackgroundSyncType sync_type,
      const base::TimeTicks& start_time,
      int number_of_batched_sync_events);

  // OnRegistrationDeleted callbacks
  void OnRegistrationDeletedImpl(int64_t sw_registration_id,
                                 base::OnceClosure callback);

  // OnStorageWiped callbacks
  void OnStorageWipedImpl(base::OnceClosure callback);

  void OnNetworkChanged();

  // SetMaxSyncAttempts callback
  void SetMaxSyncAttemptsImpl(int max_sync_attempts,
                              base::OnceClosure callback);

  // Whether an event should be logged for debuggability, for |sync_type|.
  bool ShouldLogToDevTools(blink::mojom::BackgroundSyncType sync_type);

  void ReviveOriginImpl(url::Origin origin, base::OnceClosure callback);
  void ReviveDidGetNextEventDelay(int64_t service_worker_registration_id,
                                  BackgroundSyncRegistration registration,
                                  base::OnceClosure done_closure,
                                  base::TimeDelta delay);
  void ReviveDidStoreRegistration(int64_t service_worker_registration_id,
                                  base::OnceClosure done_closure,
                                  blink::ServiceWorkerStatusCode status);
  void DidReceiveDelaysForSuspendedRegistrations(base::OnceClosure callback);

  base::OnceClosure MakeEmptyCompletion(CacheStorageSchedulerId id);

  blink::ServiceWorkerStatusCode CanEmulateSyncEvent(
      scoped_refptr<ServiceWorkerVersion> active_version);

  // Read or update |num_firing_registrations_one_shot_| or
  // |num_firing_registrations_periodic_| based on |sync_type|.
  int GetNumFiringRegistrations(blink::mojom::BackgroundSyncType sync_type);
  void UpdateNumFiringRegistrationsBy(
      blink::mojom::BackgroundSyncType sync_type,
      int to_add);

  // Map from service worker registration id to its Background Sync
  // registrations.
  std::map<int64_t, BackgroundSyncRegistrations> active_registrations_;

  CacheStorageScheduler op_scheduler_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  BackgroundSyncProxy proxy_;

  scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context_;
  std::unique_ptr<BackgroundSyncParameters> parameters_;

  // True if the manager is disabled and registrations should fail.
  bool disabled_;

  // The number of registrations currently in the firing state.
  int num_firing_registrations_one_shot_;
  int num_firing_registrations_periodic_;

  base::CancelableOnceClosure delayed_one_shot_sync_task_;
  base::CancelableOnceClosure delayed_periodic_sync_task_;

  std::unique_ptr<BackgroundSyncNetworkObserver> network_observer_;

  base::Clock* clock_;

  std::map<int64_t, int> emulated_offline_sw_;

  base::WeakPtrFactory<BackgroundSyncManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_MANAGER_H_
