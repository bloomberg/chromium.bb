// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_VIEW_API_H_
#define PPAPI_THUNK_PPB_VIEW_API_H_

#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

struct ViewData;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_View_API {
 public:
  virtual ~PPB_View_API() {}

  // Returns the view data struct. We could have virtual functions here for
  // each PPAPI function, but that would be more boilerplate for these simple
  // getters so the logic is implemented in the thunk layer. If we start
  // autogenerating the thunk layer and need this to be more regular, adding
  // the API functions here should be fine.
  virtual const ViewData& GetData() const = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif // PPAPI_THUNK_PPB_VIEW_API_H_
