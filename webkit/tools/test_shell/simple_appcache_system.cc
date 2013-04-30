// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/simple_appcache_system.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "webkit/appcache/appcache_interceptor.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using appcache::WebApplicationCacheHostImpl;
using appcache::AppCacheBackendImpl;
using appcache::AppCacheInterceptor;

// SimpleFrontendProxy --------------------------------------------------------
// Proxies method calls from the backend IO thread to the frontend UI thread.

class SimpleFrontendProxy
    : public base::RefCountedThreadSafe<SimpleFrontendProxy>,
      public appcache::AppCacheFrontend {
 public:
  explicit SimpleFrontendProxy(SimpleAppCacheSystem* appcache_system)
      : system_(appcache_system) {
  }

  void clear_appcache_system() { system_ = NULL; }

  virtual void OnCacheSelected(int host_id,
      const appcache::AppCacheInfo& info) OVERRIDE {
    if (!system_)
      return;
    if (system_->is_io_thread()) {
      system_->ui_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleFrontendProxy::OnCacheSelected, this, host_id,
                     info));
    } else if (system_->is_ui_thread()) {
      system_->frontend_impl_.OnCacheSelected(host_id, info);
    } else {
      NOTREACHED();
    }
  }

  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               appcache::Status status) OVERRIDE {
    if (!system_)
      return;
    if (system_->is_io_thread())
      system_->ui_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleFrontendProxy::OnStatusChanged, this, host_ids,
                     status));
    else if (system_->is_ui_thread())
      system_->frontend_impl_.OnStatusChanged(host_ids, status);
    else
      NOTREACHED();
  }

  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             appcache::EventID event_id) OVERRIDE {
    if (!system_)
      return;
    if (system_->is_io_thread())
      system_->ui_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleFrontendProxy::OnEventRaised, this, host_ids,
                     event_id));
    else if (system_->is_ui_thread())
      system_->frontend_impl_.OnEventRaised(host_ids, event_id);
    else
      NOTREACHED();
  }

  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete) OVERRIDE {
    if (!system_)
      return;
    if (system_->is_io_thread())
      system_->ui_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleFrontendProxy::OnProgressEventRaised, this,
                     host_ids, url, num_total, num_complete));
    else if (system_->is_ui_thread())
      system_->frontend_impl_.OnProgressEventRaised(
          host_ids, url, num_total, num_complete);
    else
      NOTREACHED();
  }

  virtual void OnErrorEventRaised(const std::vector<int>& host_ids,
                                  const std::string& message) OVERRIDE {
    if (!system_)
      return;
    if (system_->is_io_thread())
      system_->ui_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleFrontendProxy::OnErrorEventRaised, this, host_ids,
                     message));
    else if (system_->is_ui_thread())
      system_->frontend_impl_.OnErrorEventRaised(
          host_ids, message);
    else
      NOTREACHED();
  }

  virtual void OnLogMessage(int host_id,
                            appcache::LogLevel log_level,
                            const std::string& message) OVERRIDE {
    if (!system_)
      return;
    if (system_->is_io_thread())
      system_->ui_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleFrontendProxy::OnLogMessage, this, host_id,
                     log_level, message));
    else if (system_->is_ui_thread())
      system_->frontend_impl_.OnLogMessage(
          host_id, log_level, message);
    else
      NOTREACHED();
  }

  virtual void OnContentBlocked(int host_id,
                                const GURL& manifest_url) OVERRIDE {}

 private:
  friend class base::RefCountedThreadSafe<SimpleFrontendProxy>;

  virtual ~SimpleFrontendProxy() {}

  SimpleAppCacheSystem* system_;
};


// SimpleBackendProxy --------------------------------------------------------
// Proxies method calls from the frontend UI thread to the backend IO thread.

class SimpleBackendProxy
    : public base::RefCountedThreadSafe<SimpleBackendProxy>,
      public appcache::AppCacheBackend {
 public:
  explicit SimpleBackendProxy(SimpleAppCacheSystem* appcache_system)
      : system_(appcache_system), event_(true, false) {
    get_status_callback_ =
        base::Bind(&SimpleBackendProxy::GetStatusCallback,
                   base::Unretained(this));
    start_update_callback_ =
        base::Bind(&SimpleBackendProxy::StartUpdateCallback,
                   base::Unretained(this));
    swap_cache_callback_=
        base::Bind(&SimpleBackendProxy::SwapCacheCallback,
                   base::Unretained(this));
  }

  virtual void RegisterHost(int host_id) OVERRIDE {
    if (system_->is_ui_thread()) {
      system_->io_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleBackendProxy::RegisterHost, this, host_id));
    } else if (system_->is_io_thread()) {
      system_->backend_impl_->RegisterHost(host_id);
    } else {
      NOTREACHED();
    }
  }

  virtual void UnregisterHost(int host_id) OVERRIDE {
    if (system_->is_ui_thread()) {
      system_->io_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleBackendProxy::UnregisterHost, this, host_id));
    } else if (system_->is_io_thread()) {
      system_->backend_impl_->UnregisterHost(host_id);
    } else {
      NOTREACHED();
    }
  }

  virtual void SetSpawningHostId(int host_id, int spawning_host_id) OVERRIDE {
    if (system_->is_ui_thread()) {
      system_->io_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleBackendProxy::SetSpawningHostId, this, host_id,
                     spawning_host_id));
    } else if (system_->is_io_thread()) {
      system_->backend_impl_->SetSpawningHostId(host_id, spawning_host_id);
    } else {
      NOTREACHED();
    }
  }

  virtual void SelectCache(int host_id,
                           const GURL& document_url,
                           const int64 cache_document_was_loaded_from,
                           const GURL& manifest_url) OVERRIDE {
    if (system_->is_ui_thread()) {
      system_->io_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleBackendProxy::SelectCache, this, host_id,
                     document_url, cache_document_was_loaded_from,
                     manifest_url));
    } else if (system_->is_io_thread()) {
      system_->backend_impl_->SelectCache(host_id, document_url,
                                          cache_document_was_loaded_from,
                                          manifest_url);
    } else {
      NOTREACHED();
    }
  }

  virtual void GetResourceList(
      int host_id,
      std::vector<appcache::AppCacheResourceInfo>* resource_infos) OVERRIDE {
    if (system_->is_ui_thread()) {
      system_->io_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleBackendProxy::GetResourceList, this, host_id,
                     resource_infos));
    } else if (system_->is_io_thread()) {
      system_->backend_impl_->GetResourceList(host_id, resource_infos);
    } else {
      NOTREACHED();
    }
  }

  virtual void SelectCacheForWorker(
                           int host_id,
                           int parent_process_id,
                           int parent_host_id) OVERRIDE {
    NOTIMPLEMENTED();  // Workers are not supported in test_shell.
  }

  virtual void SelectCacheForSharedWorker(
                           int host_id,
                           int64 appcache_id) OVERRIDE {
    NOTIMPLEMENTED();  // Workers are not supported in test_shell.
  }

  virtual void MarkAsForeignEntry(
      int host_id,
      const GURL& document_url,
      int64 cache_document_was_loaded_from) OVERRIDE {
    if (system_->is_ui_thread()) {
      system_->io_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SimpleBackendProxy::MarkAsForeignEntry, this, host_id,
                     document_url, cache_document_was_loaded_from));
    } else if (system_->is_io_thread()) {
      system_->backend_impl_->MarkAsForeignEntry(
                                  host_id, document_url,
                                  cache_document_was_loaded_from);
    } else {
      NOTREACHED();
    }
  }

  virtual appcache::Status GetStatus(int host_id) OVERRIDE {
    if (system_->is_ui_thread()) {
      status_result_ = appcache::UNCACHED;
      event_.Reset();
      system_->io_message_loop()->PostTask(
          FROM_HERE,
              base::Bind(base::IgnoreResult(&SimpleBackendProxy::GetStatus),
                         this, host_id));
      event_.Wait();
    } else if (system_->is_io_thread()) {
      system_->backend_impl_->GetStatusWithCallback(
          host_id, get_status_callback_, NULL);
    } else {
      NOTREACHED();
    }
    return status_result_;
  }

  virtual bool StartUpdate(int host_id) OVERRIDE {
    if (system_->is_ui_thread()) {
      bool_result_ = false;
      event_.Reset();
      system_->io_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&SimpleBackendProxy::StartUpdate),
                     this, host_id));
      event_.Wait();
    } else if (system_->is_io_thread()) {
      system_->backend_impl_->StartUpdateWithCallback(
          host_id, start_update_callback_, NULL);
    } else {
      NOTREACHED();
    }
    return bool_result_;
  }

  virtual bool SwapCache(int host_id) OVERRIDE {
    if (system_->is_ui_thread()) {
      bool_result_ = false;
      event_.Reset();
      system_->io_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&SimpleBackendProxy::SwapCache),
                     this, host_id));
      event_.Wait();
    } else if (system_->is_io_thread()) {
      system_->backend_impl_->SwapCacheWithCallback(
          host_id, swap_cache_callback_, NULL);
    } else {
      NOTREACHED();
    }
    return bool_result_;
  }

  void GetStatusCallback(appcache::Status status, void* param) {
    status_result_ = status;
    event_.Signal();
  }

  void StartUpdateCallback(bool result, void* param) {
    bool_result_ = result;
    event_.Signal();
  }

  void SwapCacheCallback(bool result, void* param) {
    bool_result_ = result;
    event_.Signal();
  }

  void SignalEvent() {
    event_.Signal();
  }

 private:
  friend class base::RefCountedThreadSafe<SimpleBackendProxy>;

  virtual ~SimpleBackendProxy() {}

  SimpleAppCacheSystem* system_;
  base::WaitableEvent event_;
  bool bool_result_;
  appcache::Status status_result_;
  appcache::GetStatusCallback get_status_callback_;
  appcache::StartUpdateCallback start_update_callback_;
  appcache::SwapCacheCallback swap_cache_callback_;
};


// SimpleAppCacheSystem --------------------------------------------------------

// This class only works for a single process browser.
static const int kSingleProcessId = 1;

// A not so thread safe singleton, but should work for test_shell.
SimpleAppCacheSystem* SimpleAppCacheSystem::instance_ = NULL;

SimpleAppCacheSystem::SimpleAppCacheSystem()
    : io_message_loop_(NULL), ui_message_loop_(NULL),
      backend_proxy_(new SimpleBackendProxy(this)),
      frontend_proxy_(new SimpleFrontendProxy(this)),
      backend_impl_(NULL), service_(NULL), db_thread_("AppCacheDBThread") {
  DCHECK(!instance_);
  instance_ = this;
}

static void SignalEvent(base::WaitableEvent* event) {
  event->Signal();
}

SimpleAppCacheSystem::~SimpleAppCacheSystem() {
  DCHECK(!io_message_loop_ && !backend_impl_ && !service_);
  frontend_proxy_->clear_appcache_system();  // in case a task is in transit
  instance_ = NULL;

  if (db_thread_.IsRunning()) {
    // We pump a task thru the db thread to ensure any tasks previously
    // scheduled on that thread have been performed prior to return.
    base::WaitableEvent event(false, false);
    db_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&SignalEvent, &event));
    event.Wait();
  }
}

void SimpleAppCacheSystem::InitOnUIThread(
    const base::FilePath& cache_directory) {
  DCHECK(!ui_message_loop_);
  ui_message_loop_ = MessageLoop::current();
  cache_directory_ = cache_directory;
}

void SimpleAppCacheSystem::InitOnIOThread(
    net::URLRequestContext* request_context) {
  if (!is_initailized_on_ui_thread())
    return;

  DCHECK(!io_message_loop_);
  io_message_loop_ = MessageLoop::current();

  if (!db_thread_.IsRunning())
    db_thread_.Start();

  // Recreate and initialize per each IO thread.
  service_ = new appcache::AppCacheService(NULL);
  backend_impl_ = new appcache::AppCacheBackendImpl();
  service_->Initialize(cache_directory_,
                       db_thread_.message_loop_proxy(),
                       SimpleResourceLoaderBridge::GetCacheThread());
  service_->set_request_context(request_context);
  backend_impl_->Initialize(service_, frontend_proxy_.get(), kSingleProcessId);

  AppCacheInterceptor::EnsureRegistered();
}

void SimpleAppCacheSystem::CleanupIOThread() {
  DCHECK(is_io_thread());

  delete backend_impl_;
  delete service_;
  backend_impl_ = NULL;
  service_ = NULL;
  io_message_loop_ = NULL;

  // Just in case the main thread is waiting on it.
  backend_proxy_->SignalEvent();
}

WebApplicationCacheHost* SimpleAppCacheSystem::CreateCacheHostForWebKit(
    WebApplicationCacheHostClient* client) {
  if (!is_initailized_on_ui_thread())
    return NULL;

  DCHECK(is_ui_thread());

  // The IO thread needs to be running for this system to work.
  SimpleResourceLoaderBridge::EnsureIOThread();

  if (!is_initialized())
    return NULL;
  return new WebApplicationCacheHostImpl(client, backend_proxy_.get());
}

void SimpleAppCacheSystem::SetExtraRequestBits(
    net::URLRequest* request, int host_id, ResourceType::Type resource_type) {
  if (is_initialized()) {
    DCHECK(is_io_thread());
    AppCacheInterceptor::SetExtraRequestInfo(
        request, service_, kSingleProcessId, host_id, resource_type);
  }
}

void SimpleAppCacheSystem::GetExtraResponseBits(
    net::URLRequest* request, int64* cache_id, GURL* manifest_url) {
  if (is_initialized()) {
    DCHECK(is_io_thread());
    AppCacheInterceptor::GetExtraResponseInfo(
        request, cache_id, manifest_url);
  }
}
