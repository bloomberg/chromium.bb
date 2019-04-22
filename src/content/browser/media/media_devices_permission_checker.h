// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_PERMISSION_CHECKER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_PERMISSION_CHECKER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/common/content_export.h"

namespace content {

// This class provides various utility functions to check if a render frame
// has permission to access media devices. Note that none of the methods
// prompts the user to request permission.
class CONTENT_EXPORT MediaDevicesPermissionChecker {
 public:
  MediaDevicesPermissionChecker();
  // This constructor creates a MediaDevicesPermissionChecker that replies
  // |override_value| to all permission requests. Use only for testing.
  explicit MediaDevicesPermissionChecker(bool override_value);

  // Checks if the origin associated to a render frame identified by
  // |render_process_id| and |render_frame_id| is allowed to access the media
  // device type |device_type|.
  // This method must be called on the UI thread.
  bool CheckPermissionOnUIThread(blink::MediaDeviceType device_type,
                                 int render_process_id,
                                 int render_frame_id) const;

  // Checks if the origin associated to a render frame identified by
  // |render_process_id| and |render_frame_id| is allowed to access the media
  // device type |device_type|. The result is passed to |callback|.
  // This method can be called on any thread. |callback| is fired on the same
  // thread this method is called on.
  void CheckPermission(blink::MediaDeviceType device_type,
                       int render_process_id,
                       int render_frame_id,
                       base::OnceCallback<void(bool)> callback) const;

  // Checks if the origin associated to a render frame identified by
  // |render_process_id| and |render_frame_id| is allowed to access the media
  // device types marked with a value of true in |requested_device_types|. The
  // result is passed to |callback|. The result is indexed by
  // blink::MediaDeviceType. Entries in the result with a value of true for
  // requested device types indicate that the frame has permission to access
  // devices of the corresponding types. This method can be called on any
  // thread. |callback| is fired on the same thread this method is called on.
  void CheckPermissions(
      MediaDevicesManager::BoolDeviceTypes requested_device_types,
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(const MediaDevicesManager::BoolDeviceTypes&)>
          callback) const;

 private:
  const bool use_override_;
  const bool override_value_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesPermissionChecker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_PERMISSION_CHECKER_H_
