/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/public/irt_core.h"

/*
 * TODO(mseaborn): The "weak" attribute can be removed after the
 * Chromium side defines this function, then this source file can be
 * removed from "irt_browser_lib" in irt.gyp.
 */
__attribute__((weak))
void nacl_irt_start(uint32_t *info) {
  nacl_irt_init(info);
  nacl_irt_enter_user_code(info, nacl_irt_query_core);
}
