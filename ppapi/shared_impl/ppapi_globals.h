// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_GLOBALS_H_
#define PPAPI_SHARED_IMPL_PPAPI_GLOBALS_H_

#include "base/basictypes.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class ResourceTracker;
class VarTracker;

// Abstract base class
class PPAPI_SHARED_EXPORT PpapiGlobals {
 public:
  PpapiGlobals();
  virtual ~PpapiGlobals();

  // Getter for the global singleton.
  inline static PpapiGlobals* Get() { return ppapi_globals_; }

  virtual ResourceTracker* GetResourceTracker() = 0;
  virtual VarTracker* GetVarTracker() = 0;

 private:
  static PpapiGlobals* ppapi_globals_;

  DISALLOW_COPY_AND_ASSIGN(PpapiGlobals);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_GLOBALS_H_
