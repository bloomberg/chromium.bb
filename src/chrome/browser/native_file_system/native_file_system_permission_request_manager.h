// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_REQUEST_MANAGER_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_REQUEST_MANAGER_H_

#include "base/callback_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace permissions {
enum class PermissionAction;
}

// This class manages native file system permission requests for a particular
// WebContents. It is very similar to the generic PermissionRequestManager
// class, and as such deals with throttling, coallescing and/or completely
// denying permission requests, depending on the situation and policy.
//
// Native File System code doesn't just use PermissionRequestManager directly
// because the permission requests use different UI, and as such can't easily
// be supported by PermissionRequestManager.
//
// The NativeFileSystemPermissionRequestManager should be used on the UI thread.
class NativeFileSystemPermissionRequestManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          NativeFileSystemPermissionRequestManager> {
 public:
  ~NativeFileSystemPermissionRequestManager() override;

  enum class Access {
    // Only ask for read access.
    kRead,
    // Only ask for write access, assuming read access has already been granted.
    kWrite,
    // Ask for both read and write access.
    kReadWrite
  };

  struct RequestData {
    RequestData(const url::Origin& origin,
                const base::FilePath& path,
                bool is_directory,
                Access access)
        : origin(origin),
          path(path),
          is_directory(is_directory),
          access(access) {}
    RequestData(RequestData&&) = default;
    RequestData(const RequestData&) = default;
    RequestData& operator=(RequestData&&) = default;
    RequestData& operator=(const RequestData&) = default;

    url::Origin origin;
    base::FilePath path;
    bool is_directory;
    Access access;
  };

  void AddRequest(
      RequestData request,
      base::OnceCallback<void(permissions::PermissionAction result)> callback,
      base::ScopedClosureRunner fullscreen_block);

  // Do NOT use this method in production code. Use this method in browser
  // tests that need to accept or deny permissions when requested in
  // JavaScript. Your test needs to call this before permission is requested,
  // and then the bubble will proceed as desired as soon as it would have been
  // shown.
  void set_auto_response_for_test(
      base::Optional<permissions::PermissionAction> response) {
    auto_response_for_test_ = response;
  }

 private:
  friend class content::WebContentsUserData<
      NativeFileSystemPermissionRequestManager>;

  explicit NativeFileSystemPermissionRequestManager(
      content::WebContents* web_contents);

  bool IsShowingRequest() const { return current_request_ != nullptr; }
  bool CanShowRequest() const;
  void ScheduleShowRequest();
  void DequeueAndShowRequest();

  // WebContentsObserver
  void DocumentOnLoadCompletedInMainFrame() override;

  void OnPermissionDialogResult(permissions::PermissionAction result);

  struct Request;
  // Request currently being shown in prompt.
  std::unique_ptr<Request> current_request_;
  // Queued up requests.
  base::circular_deque<std::unique_ptr<Request>> queued_requests_;

  // We only show new prompts when this is true.
  bool main_frame_has_fully_loaded_ = false;

  base::Optional<permissions::PermissionAction> auto_response_for_test_;

  base::WeakPtrFactory<NativeFileSystemPermissionRequestManager> weak_factory_{
      this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_REQUEST_MANAGER_H_
