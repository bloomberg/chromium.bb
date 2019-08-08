// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_NAVIGATION_HANDLE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_NAVIGATION_HANDLE_H_

#include <memory>
#include "base/macros.h"
#include "base/unguessable_token.h"

namespace content {

class AppCacheNavigationHandleCore;
class ChromeAppCacheService;

// This class is used to manage the lifetime of AppCacheHosts created during
// navigation. This is a UI thread class, with a pendant class on the IO
// thread, the AppCacheNavigationHandleCore.
//
// The lifetime of the AppCacheNavigationHandle, the
// AppCacheNavigationHandleCore and the AppCacheHost are the following :
//   1) We create a AppCacheNavigationHandle on the UI thread with a
//      app cache host id of -1. This also leads to the creation of a
//      AppCacheNavigationHandleCore with an id of -1. Every time
//      an AppCacheNavigationHandle instance is created the global host id is
//      decremented by 1.
//
//   2) When the navigation request is sent to the IO thread, we include a
//      pointer to the AppCacheNavigationHandleCore.
//
//   3) The AppCacheHost instance is created and its ownership is passed to the
//      AppCacheNavigationHandleCore instance.
//
//   4) When the navigation is ready to commit, the NavigationRequest will
//      update the CommitNavigationParams based on the id from the
//      AppCacheNavigationHandle.
//
//   5) The commit leads to AppCache registrations happening from the renderer.
//      This is via the AppCacheBackend.RegisterHost mojo call. The
//      AppCacheBackendImpl class which handles these calls will be informed
//      about these hosts when the navigation commits. It will ignore the
//      host registrations as they have already been registered. The
//      ownership of the AppCacheHost is passed from the
//      AppCacheNavigationHandle core to the AppCacheBackendImpl.
//
//   6) Meanwhile, RenderFrameHostImpl takes ownership of
//      AppCacheNavigationHandle once navigation commits, so that precreated
//      AppCacheHost is not destroyed before IPC above reaches AppCacheBackend.
//
//   7) When the next navigation commits, previous AppCacheNavigationHandle is
//      destroyed. The destructor of the AppCacheNavigationHandle posts a
//      task to destroy the AppCacheNavigationHandleCore on the IO thread.

class AppCacheNavigationHandle {
 public:
  AppCacheNavigationHandle(ChromeAppCacheService* appcache_service,
                           int process_id);
  ~AppCacheNavigationHandle();

  const base::UnguessableToken& appcache_host_id() const {
    return appcache_host_id_;
  }
  AppCacheNavigationHandleCore* core() const { return core_.get(); }

  // Called by NavigationHandleImpl::ReadyToCommitNavigation, because this is
  // the earliest time when the process id is known.  NavigationHandleImpl needs
  // to construct AppCacheNavigationHandle at the start of a navigation (when
  // the final process might not be known yet) and therefore temporarily
  // constructs AppCacheNavigationHandle with an invalid process id.
  //
  // SetProcessId may only be called once, and only if kInvalidUniqueID was
  // passed to the AppCacheNavigationHandle's constructor.
  void SetProcessId(int process_id);

 private:
  const base::UnguessableToken appcache_host_id_;
  std::unique_ptr<AppCacheNavigationHandleCore> core_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheNavigationHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_NAVIGATION_HANDLE_H_
