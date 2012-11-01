// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_FLASH_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_FLASH_SHARED_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/thunk/ppb_flash_api.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_Flash_Shared : public thunk::PPB_Flash_API {
 public:
  PPB_Flash_Shared();
  virtual ~PPB_Flash_Shared();

  // Shared implementation of PPB_Flash_API.
  virtual void FreeDirContents(PP_Instance instance,
                               PP_DirContents_Dev* contents) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_FLASH_SHARED_H_
