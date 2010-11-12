/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/fake_resource.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/tests/fake_browser_ppapi/fake_file_io.h"
#include "native_client/tests/fake_browser_ppapi/fake_url_loader.h"
#include "native_client/tests/fake_browser_ppapi/fake_url_request_info.h"
#include "native_client/tests/fake_browser_ppapi/fake_url_response_info.h"

namespace fake_browser_ppapi {

Resource Resource::kInvalidResource = Resource();

}  // namespace fake_browser_ppapi
