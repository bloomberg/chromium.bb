// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_flash_shared.h"

namespace ppapi {

PPB_Flash_Shared::PPB_Flash_Shared() {
}

PPB_Flash_Shared::~PPB_Flash_Shared() {
}

void PPB_Flash_Shared::FreeDirContents(PP_Instance instance,
                                       PP_DirContents_Dev* contents) {
  for (int32_t i = 0; i < contents->count; ++i)
    delete[] contents->entries[i].name;
  delete[] contents->entries;
  delete contents;
}

}  // namespace ppapi
