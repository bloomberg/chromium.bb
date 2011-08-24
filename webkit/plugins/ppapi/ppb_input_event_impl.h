// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/input_event_impl.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {
struct InputEventData;
}

namespace webkit {
namespace ppapi {

class PPB_InputEvent_Impl : public ::ppapi::Resource,
                            public ::ppapi::InputEventImpl {
 public:
  PPB_InputEvent_Impl(PP_Instance instance,
                      const ::ppapi::InputEventData& data);

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_InputEvent_API* AsPPB_InputEvent_API() OVERRIDE;

 protected:
  // InputEventImpl implementation.
  virtual PP_Var StringToPPVar(const std::string& str) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_InputEvent_Impl);
};

}  // namespace ppapi
}  // namespace webkit

