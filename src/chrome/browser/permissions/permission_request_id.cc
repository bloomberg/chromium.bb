// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_id.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

PermissionRequestID::PermissionRequestID(
    content::RenderFrameHost* render_frame_host,
    int request_id)
    : render_process_id_(render_frame_host->GetProcess()->GetID()),
      render_frame_id_(render_frame_host->GetRoutingID()),
      request_id_(request_id) {
}

PermissionRequestID::PermissionRequestID(int render_process_id,
                                         int render_frame_id,
                                         int request_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      request_id_(request_id) {
}

PermissionRequestID::~PermissionRequestID() {
}

bool PermissionRequestID::operator==(const PermissionRequestID& other) const {
  return render_process_id_ == other.render_process_id_ &&
         render_frame_id_ == other.render_frame_id_ &&
         request_id_ == other.request_id_;
}

bool PermissionRequestID::operator!=(const PermissionRequestID& other) const {
  return !operator==(other);
}

std::string PermissionRequestID::ToString() const {
  return base::StringPrintf("%d,%d,%d",
                            render_process_id_,
                            render_frame_id_,
                            request_id_);
}
