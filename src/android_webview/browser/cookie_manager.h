// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COOKIE_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_COOKIE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class CookieStore;
}

namespace network {
class CookieManager;
}

namespace android_webview {

class BoolCookieCallbackHolder;

// CookieManager creates and owns Webview's CookieStore, in addition to handling
// calls into the CookieStore from Java.
//
// Since Java calls can be made on the IO Thread, and must synchronously return
// a result, and the CookieStore API allows it to asynchronously return results,
// the CookieStore must be run on its own thread, to prevent deadlock.
class CookieManager {
 public:
  static CookieManager* GetInstance();

  // Returns the TaskRunner on which the CookieStore lives.
  base::SingleThreadTaskRunner* GetCookieStoreTaskRunner();
  // Returns the CookieStore, creating it if necessary. This must only be called
  // on the CookieStore TaskRunner.
  net::CookieStore* GetCookieStore();
  // Passes a |cookie_manager_info|, which this will use for CookieManager APIs
  // going forward. Only called in the Network Service path, with the intention
  // this is called once during content initialization (when we create the
  // only NetworkContext). Note: no other cookie tasks will be processed while
  // this operation is running.
  void SetMojoCookieManager(
      network::mojom::CookieManagerPtrInfo cookie_manager_info);

  void SetShouldAcceptCookies(bool accept);
  bool GetShouldAcceptCookies();
  void SetCookie(const GURL& host,
                 const std::string& cookie_value,
                 std::unique_ptr<BoolCookieCallbackHolder> callback);
  void SetCookieSync(const GURL& host, const std::string& cookie_value);
  std::string GetCookie(const GURL& host);
  void RemoveSessionCookies(std::unique_ptr<BoolCookieCallbackHolder> callback);
  void RemoveAllCookies(std::unique_ptr<BoolCookieCallbackHolder> callback);
  void RemoveAllCookiesSync();
  void RemoveSessionCookiesSync();
  void RemoveExpiredCookies();
  void FlushCookieStore();
  bool HasCookies();
  bool AllowFileSchemeCookies();
  void SetAcceptFileSchemeCookies(bool accept);

 private:
  friend struct base::LazyInstanceTraitsBase<CookieManager>;

  CookieManager();
  ~CookieManager();

  // Gets the Network Service CookieManager if it's been passed via
  // |SetMojoCookieManager|. Otherwise (if Network Service is disabled or
  // content layer has not yet initialized the NetworkContext), this returns
  // nullptr (and |GetCookieStore| should be used installed). This must only be
  // called on the CookieStore TaskRunner.
  network::mojom::CookieManager* GetMojoCookieManager();

  void ExecCookieTaskSync(
      base::OnceCallback<void(base::RepeatingCallback<void(bool)>)> task);
  void ExecCookieTaskSync(
      base::OnceCallback<void(base::RepeatingCallback<void(int)>)> task);
  void ExecCookieTaskSync(base::OnceCallback<void(base::OnceClosure)> task);
  void ExecCookieTask(base::OnceClosure task);
  // Runs all queued-up cookie tasks in |tasks_|.
  void RunPendingCookieTasks();

  void SetCookieHelper(const GURL& host,
                       const std::string& value,
                       base::RepeatingCallback<void(bool)> callback);

  void GotCookies(const std::vector<net::CanonicalCookie>& cookies);
  void GetCookieListAsyncHelper(const GURL& host,
                                net::CookieList* result,
                                base::OnceClosure complete);
  void GetCookieListCompleted(base::OnceClosure complete,
                              net::CookieList* result,
                              const net::CookieList& value,
                              const net::CookieStatusList& excluded_cookies);

  void RemoveSessionCookiesHelper(base::RepeatingCallback<void(bool)> callback);
  void RemoveAllCookiesHelper(base::RepeatingCallback<void(bool)> callback);
  void RemoveCookiesCompleted(base::RepeatingCallback<void(bool)> callback,
                              uint32_t num_deleted);

  void FlushCookieStoreAsyncHelper(base::OnceClosure complete);

  void SetMojoCookieManagerAsync(
      network::mojom::CookieManagerPtrInfo cookie_manager_info,
      base::OnceClosure complete);
  void SwapMojoCookieManagerAsync(
      network::mojom::CookieManagerPtrInfo cookie_manager_info,
      base::OnceClosure complete);

  void HasCookiesAsyncHelper(bool* result, base::OnceClosure complete);
  void HasCookiesCompleted(base::OnceClosure complete,
                           bool* result,
                           const net::CookieList& cookies,
                           const net::CookieStatusList& excluded_cookies);
  void HasCookiesCompleted2(base::OnceClosure complete,
                            bool* result,
                            const net::CookieList& cookies);

  // |result| indicates whether or not this call was successful, indicating
  // whether we may update |accept_file_scheme_cookies_|.
  void AllowFileSchemeCookiesAsyncHelper(bool accept,
                                         bool* result,
                                         base::OnceClosure complete);
  void AllowFileSchemeCookiesCompleted(base::OnceClosure complete,
                                       bool* result,
                                       bool value);

  // This protects the following two bools, as they're used on multiple threads.
  base::Lock accept_file_scheme_cookies_lock_;
  // True if cookies should be allowed for file URLs. Can only be changed prior
  // to creating the CookieStore.
  bool accept_file_scheme_cookies_;
  // True once the cookie store has been created. Just used to track when
  // |accept_file_scheme_cookies_| can no longer be modified.
  bool cookie_store_created_;

  base::Thread cookie_store_client_thread_;
  base::Thread cookie_store_backend_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> cookie_store_task_runner_;
  std::unique_ptr<net::CookieStore> cookie_store_;

  // Tracks if we're in the middle of a call to SetMojoCookieManager(). See the
  // note in SetMojoCookieManager(). Must only be accessed on
  // |cookie_store_task_runner_|.
  bool setting_new_mojo_cookie_manager_;

  // |tasks_| is a queue we manage, to allow us to delay tasks until after
  // SetMojoCookieManager()'s work is done. This is modified on different
  // threads, so accesses must be guarded by |task_queue_lock_|.
  base::Lock task_queue_lock_;
  base::circular_deque<base::OnceClosure> tasks_;

  // The CookieManager shared with the NetworkContext.
  network::mojom::CookieManagerPtr mojo_cookie_manager_;

  DISALLOW_COPY_AND_ASSIGN(CookieManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COOKIE_MANAGER_H_
