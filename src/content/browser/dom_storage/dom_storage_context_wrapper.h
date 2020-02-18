// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/dom_storage_context.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

namespace base {
class FilePath;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace content {

class SessionStorageContextMojo;
class SessionStorageNamespaceImpl;

// This is owned by Storage Partition and encapsulates all its dom storage
// state.
// Can only be accessed on the UI thread, except for the AddNamespace and
// RemoveNamespace methods.
class CONTENT_EXPORT DOMStorageContextWrapper
    : public DOMStorageContext,
      public base::RefCountedThreadSafe<DOMStorageContextWrapper> {
 public:
  // Option for PurgeMemory.
  enum PurgeOption {
    // Determines if purging is required based on the usage and the platform.
    PURGE_IF_NEEDED,

    // Purge unopened areas only.
    PURGE_UNOPENED,

    // Purge aggressively, i.e. discard cache even for areas that have
    // non-zero open count.
    PURGE_AGGRESSIVE,
  };

  // If |profile_path| is empty, nothing will be saved to disk.
  static scoped_refptr<DOMStorageContextWrapper> Create(
      const base::FilePath& profile_path,
      const base::FilePath& local_partition_path,
      storage::SpecialStoragePolicy* special_storage_policy);

  DOMStorageContextWrapper(
      scoped_refptr<base::SequencedTaskRunner> mojo_task_runner,
      SessionStorageContextMojo* mojo_session_storage_context,
      mojo::Remote<storage::mojom::LocalStorageControl> local_storage_control);

  storage::mojom::LocalStorageControl* GetLocalStorageControl();

  // DOMStorageContext implementation.
  void GetLocalStorageUsage(GetLocalStorageUsageCallback callback) override;
  void GetSessionStorageUsage(GetSessionStorageUsageCallback callback) override;
  void DeleteLocalStorage(const url::Origin& origin,
                          base::OnceClosure callback) override;
  void PerformLocalStorageCleanup(base::OnceClosure callback) override;
  void DeleteSessionStorage(const SessionStorageUsageInfo& usage_info,
                            base::OnceClosure callback) override;
  void PerformSessionStorageCleanup(base::OnceClosure callback) override;
  scoped_refptr<SessionStorageNamespace> RecreateSessionStorage(
      const std::string& namespace_id) override;
  void StartScavengingUnusedSessionStorage() override;

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState();

  // Called when the BrowserContext/Profile is going away.
  void Shutdown();

  void Flush();

  void OpenLocalStorage(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver);
  void OpenSessionStorage(
      int process_id,
      const std::string& namespace_id,
      mojo::ReportBadMessageCallback bad_message_callback,
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver);

  SessionStorageContextMojo* mojo_session_state() {
    return mojo_session_state_;
  }

 private:
  friend class DOMStorageMessageFilter;  // for access to context()
  friend class SessionStorageNamespaceImpl;  // ditto
  friend class base::RefCountedThreadSafe<DOMStorageContextWrapper>;
  friend class DOMStorageBrowserTest;

  ~DOMStorageContextWrapper() override;

  base::SequencedTaskRunner* mojo_task_runner() {
    return mojo_task_runner_.get();
  }

  scoped_refptr<SessionStorageNamespaceImpl> MaybeGetExistingNamespace(
      const std::string& namespace_id) const;

  // Note: can be called on multiple threads, protected by a mutex.
  void AddNamespace(const std::string& namespace_id,
                    SessionStorageNamespaceImpl* session_namespace);

  // Note: can be called on multiple threads, protected by a mutex.
  void RemoveNamespace(const std::string& namespace_id);

  // Called on UI thread when the system is under memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  void PurgeMemory(PurgeOption purge_option);

  // Keep all mojo-ish details together and not bleed them through the public
  // interface. The |mojo_session_state_| object is owned by this object, but
  // destroyed asynchronously on the |mojo_task_runner_|.
  SessionStorageContextMojo* mojo_session_state_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> mojo_task_runner_;

  // Since the tab restore code keeps a reference to the session namespaces
  // of recently closed tabs (see sessions::ContentPlatformSpecificTabData and
  // sessions::TabRestoreService), a SessionStorageNamespaceImpl can outlive the
  // destruction of the browser window. A session restore can also happen
  // without the browser context being shutdown or destroyed in between. The
  // design of SessionStorageNamespaceImpl assumes there is only one object per
  // namespace. A session restore creates new objects for all tabs while the
  // Profile wasn't destructed. This map allows the restored session to re-use
  // the SessionStorageNamespaceImpl objects that are still alive thanks to the
  // sessions component.
  std::map<std::string, SessionStorageNamespaceImpl*> alive_namespaces_
      GUARDED_BY(alive_namespaces_lock_);
  mutable base::Lock alive_namespaces_lock_;

  // To receive memory pressure signals.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Connection to the partition's LocalStorageControl interface within the
  // Storage Service.
  mojo::Remote<storage::mojom::LocalStorageControl> local_storage_control_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContextWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
