// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_sync_context.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace content {

class BackgroundSyncManager;
class BackgroundSyncServiceImpl;
class DevToolsBackgroundServicesContextImpl;
class ServiceWorkerContextWrapper;

// One instance of this exists per StoragePartition, and services multiple child
// processes/origins. Most logic is delegated to the owned BackgroundSyncManager
// instance, which is only accessed on the IO thread.
class CONTENT_EXPORT BackgroundSyncContextImpl
    : public BackgroundSyncContext,
      public base::RefCountedDeleteOnSequence<BackgroundSyncContextImpl> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  BackgroundSyncContextImpl();

  // Init and Shutdown are for use on the UI thread when the
  // StoragePartition is being setup and torn down.
  void Init(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
      const scoped_refptr<DevToolsBackgroundServicesContextImpl>&
          devtools_context);

  // Shutdown must be called before deleting this. Call on the UI thread.
  void Shutdown();

  // Create a BackgroundSyncServiceImpl that is owned by this. Call on the UI
  // thread.
  void CreateService(blink::mojom::BackgroundSyncServiceRequest request);

  // Called by BackgroundSyncServiceImpl objects so that they can
  // be deleted. Call on the IO thread.
  void ServiceHadConnectionError(BackgroundSyncServiceImpl* service);

  // Call on the IO thread.
  BackgroundSyncManager* background_sync_manager() const;

  // BackgroundSyncContext implementation.
  void FireBackgroundSyncEvents(base::OnceClosure done_closure) override;
  void GetSoonestWakeupDelta(
      base::OnceCallback<void(base::TimeDelta)> callback) override;

 protected:
  friend class base::RefCountedDeleteOnSequence<BackgroundSyncContextImpl>;
  friend class base::DeleteHelper<BackgroundSyncContextImpl>;
  ~BackgroundSyncContextImpl() override;

  void set_background_sync_manager_for_testing(
      std::unique_ptr<BackgroundSyncManager> manager);
  void set_wakeup_delta_for_testing(base::TimeDelta wakeup_delta);

 private:
  friend class BackgroundSyncServiceImplTest;
  friend class BackgroundSyncLauncherTest;

  void FireBackgroundSyncEventsOnIOThread(base::OnceClosure done_closure);
  void DidFireBackgroundSyncEventsOnIOThread(base::OnceClosure done_closure);
  virtual void CreateBackgroundSyncManager(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context);

  void CreateServiceOnIOThread(
      mojo::InterfaceRequest<blink::mojom::BackgroundSyncService> request);

  void ShutdownOnIO();

  base::TimeDelta GetSoonestWakeupDeltaOnIOThread();
  void DidGetSoonestWakeupDelta(
      base::OnceCallback<void(base::TimeDelta)> callback,
      base::TimeDelta soonest_wakeup_delta);

  // Only accessed on the IO thread.
  std::unique_ptr<BackgroundSyncManager> background_sync_manager_;

  // The services are owned by this. They're either deleted
  // during ShutdownOnIO or when the channel is closed via
  // ServiceHadConnectionError. Only accessed on the IO thread.
  std::map<BackgroundSyncServiceImpl*,
           std::unique_ptr<BackgroundSyncServiceImpl>>
      services_;
  base::TimeDelta test_wakeup_delta_ = base::TimeDelta::Max();

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
