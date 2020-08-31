// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains callback types used for communicating with the Drive
// server via WAPI (Documents List API) and Drive API.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_COMMON_CALLBACKS_H_
#define GOOGLE_APIS_DRIVE_DRIVE_COMMON_CALLBACKS_H_

#include "google_apis/drive/base_requests.h"

namespace google_apis {

class AboutResource;

// Callback used for getting AboutResource.
typedef base::OnceCallback<void(DriveApiErrorCode error,
                                std::unique_ptr<AboutResource> about_resource)>
    AboutResourceCallback;

// Closure for canceling a certain request. Each request-issuing method returns
// this type of closure. If it is called during the request is in-flight, the
// callback passed with the request is invoked with DRIVE_CANCELLED. If the
// request is already finished, nothing happens.
typedef base::Closure CancelCallback;

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_COMMON_CALLBACKS_H_
