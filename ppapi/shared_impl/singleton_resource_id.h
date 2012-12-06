// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_SINGLETON_RESOURCE_ID_H_
#define PPAPI_SHARED_IMPL_SINGLETON_RESOURCE_ID_H_

namespace ppapi {

// These IDs are used to access singleton resource objects using
// PPB_Instance_API.GetSingletonResource.
enum SingletonResourceID {
  FLASH_CLIPBOARD_SINGLETON_ID,
  FLASH_FILE_SINGLETON_ID,
  FLASH_FULLSCREEN_SINGLETON_ID,
  FLASH_SINGLETON_ID,
  GAMEPAD_SINGLETON_ID,
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_SINGLETON_RESOURCE_ID_H_
