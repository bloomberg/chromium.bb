// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/vmo_util.h"

#include <string>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/stl_util.h"

namespace webrunner {

bool ReadUTF8FromVMOAsUTF16(const fuchsia::mem::Buffer& buffer,
                            base::string16* output) {
  std::string vmo_data;
  vmo_data.resize(buffer.size);
  zx_status_t status =
      buffer.vmo.read(base::data(vmo_data), 0, vmo_data.size());
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_vmo_read";
    return false;
  }
  return base::UTF8ToUTF16(&vmo_data.front(), vmo_data.size(), output);
}

}  // namespace webrunner
