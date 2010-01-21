// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_APPCACHE_SYSTEM_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_APPCACHE_SYSTEM_H_

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_frontend_impl.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_thread.h"
#include "webkit/glue/resource_type.h"

namespace WebKit {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
}
class SimpleBackendProxy;
class SimpleFrontendProxy;
class URLRequest;
class URLRequestContext;

// A class that composes the constituent parts of an appcache system
// together for use in a single process with two relavant threads,
// a UI thread on which webkit runs and an IO thread on which URLRequests
// are handled. This class conspires with SimpleResourceLoaderBridge to
// retrieve resources from the appcache.
class SimpleAppCacheSystem : public MessageLoop::DestructionObserver {
 public:
  // Should be instanced somewhere in main(). If not instanced, the public
  // static methods are all safe no-ops.
  SimpleAppCacheSystem();
  virtual ~SimpleAppCacheSystem();

  // One-time main UI thread initialization.
  static void InitializeOnUIThread(const FilePath& cache_directory) {
    if (instance_)
      instance_->InitOnUIThread(cache_directory);
  }

  // Called by SimpleResourceLoaderBridge's IOThread class.
  // Per IO thread initialization. Only one IO thread can exist
  // at a time, but after IO thread termination a new one can be
  // started on which this method should be called. The instance
  // is assumed to outlive the IO thread.
  static void InitializeOnIOThread(URLRequestContext* request_context) {
    if (instance_)
      instance_->InitOnIOThread(request_context);
  }

  // Called by TestShellWebKitInit to manufacture a 'host' for webcore.
  static WebKit::WebApplicationCacheHost* CreateApplicationCacheHost(
      WebKit::WebApplicationCacheHostClient* client) {
    return instance_ ? instance_->CreateCacheHostForWebKit(client) : NULL;
  }

  // Called by SimpleResourceLoaderBridge to hook into resource loads.
  static void SetExtraRequestInfo(URLRequest* request,
                                  int host_id,
                                  ResourceType::Type resource_type) {
    if (instance_)
      instance_->SetExtraRequestBits(request, host_id, resource_type);
  }

  // Called by SimpleResourceLoaderBridge extract extra response bits.
  static void GetExtraResponseInfo(URLRequest* request,
                            int64* cache_id,
                            GURL* manifest_url) {
    if (instance_)
      instance_->GetExtraResponseBits(request, cache_id, manifest_url);
  }

  // Some unittests create their own IO and DB threads.

  enum AppCacheThreadID {
    DB_THREAD_ID,
    IO_THREAD_ID,
  };

  class ThreadProvider {
   public:
    virtual ~ThreadProvider() {}
    virtual bool PostTask(
        int id,
        const tracked_objects::Location& from_here,
        Task* task) = 0;
    virtual bool CurrentlyOn(int id) = 0;
  };

  static void set_thread_provider(ThreadProvider* provider) {
    DCHECK(instance_);
    DCHECK(!provider || !instance_->thread_provider_);
    instance_->thread_provider_ = provider;
  }

  static ThreadProvider* thread_provider() {
    return instance_ ? instance_->thread_provider_ : NULL;
  }

 private:
  friend class SimpleBackendProxy;
  friend class SimpleFrontendProxy;
  friend class appcache::AppCacheThread;

  // Instance methods called by our static public methods
  void InitOnUIThread(const FilePath& cache_directory);
  void InitOnIOThread(URLRequestContext* request_context);
  WebKit::WebApplicationCacheHost* CreateCacheHostForWebKit(
      WebKit::WebApplicationCacheHostClient* client);
  void SetExtraRequestBits(URLRequest* request,
                           int host_id,
                           ResourceType::Type resource_type);
  void GetExtraResponseBits(URLRequest* request,
                            int64* cache_id,
                            GURL* manifest_url);

  // Helpers
  MessageLoop* io_message_loop() { return io_message_loop_; }
  MessageLoop* ui_message_loop() { return ui_message_loop_; }
  bool is_io_thread() { return MessageLoop::current() == io_message_loop_; }
  bool is_ui_thread() { return MessageLoop::current() == ui_message_loop_; }
  bool is_initialized() {
    return io_message_loop_ && is_initailized_on_ui_thread();
  }
  bool is_initailized_on_ui_thread() {
    return ui_message_loop_ ? true : false;
  }
  static MessageLoop* GetMessageLoop(int id) {
    if (instance_) {
      if (id == IO_THREAD_ID)
        return instance_->io_message_loop_;
      if (id == DB_THREAD_ID)
        return instance_->db_thread_.message_loop();
      NOTREACHED() << "Invalid AppCacheThreadID value";
    }
    return NULL;
  }

  // IOThread DestructionObserver
  virtual void WillDestroyCurrentMessageLoop();

  FilePath cache_directory_;
  MessageLoop* io_message_loop_;
  MessageLoop* ui_message_loop_;
  scoped_refptr<SimpleBackendProxy> backend_proxy_;
  scoped_refptr<SimpleFrontendProxy> frontend_proxy_;
  appcache::AppCacheFrontendImpl frontend_impl_;

  // Created and used only on the IO thread, these do
  // not survive IO thread termination. If a new IO thread
  // is started new instances will be created.
  appcache::AppCacheBackendImpl* backend_impl_;
  appcache::AppCacheService* service_;

  // We start a thread for use as the DB thread.
  base::Thread db_thread_;

  // Some unittests create there own IO and DB threads.
  ThreadProvider* thread_provider_;

  // A low-tech singleton.
  static SimpleAppCacheSystem* instance_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_APPCACHE_SYSTEM_H_
