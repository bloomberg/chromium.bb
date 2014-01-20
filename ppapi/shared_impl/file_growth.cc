// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_growth.h"

#include "base/logging.h"

namespace ppapi {

FileGrowth::FileGrowth()
    : max_written_offset(0),
      append_mode_write_amount(0) {
}

FileGrowth::FileGrowth(int64_t max_written_offset,
                       int64_t append_mode_write_amount)
    : max_written_offset(max_written_offset),
      append_mode_write_amount(append_mode_write_amount) {
  DCHECK_LE(0, max_written_offset);
  DCHECK_LE(0, append_mode_write_amount);
}

}  // namespace ppapi
