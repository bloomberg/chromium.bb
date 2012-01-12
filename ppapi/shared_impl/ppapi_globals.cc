// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_globals.h"

#include "base/logging.h"

namespace ppapi {

PpapiGlobals* PpapiGlobals::ppapi_globals_ = NULL;

PpapiGlobals::PpapiGlobals() {
  DCHECK(!ppapi_globals_);
  ppapi_globals_ = this;
}

PpapiGlobals::~PpapiGlobals() {
  DCHECK(ppapi_globals_ == this);
  ppapi_globals_ = NULL;
}

}  // namespace ppapi
