// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_INTERFACE_IMPL_H_
#define WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_INTERFACE_IMPL_H_

#include "build/build_config.h"

#if defined(OS_LINUX)
#include "webkit/fileapi/media/mtp_device_interface_impl_linux.h"
#endif

namespace fileapi {

// TODO(kmadhusu): Implement mtp device interface on other platforms.
#if defined(OS_LINUX)
  typedef class MtpDeviceInterfaceImplLinux MediaDeviceInterfaceImpl;
#endif

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_INTERFACE_IMPL_H_
