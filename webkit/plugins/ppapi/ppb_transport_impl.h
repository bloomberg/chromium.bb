// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_TRANSPORT_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_TRANSPORT_IMPL_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/pp_instance.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/resource.h"

struct PPB_Transport_Dev;

namespace webkit {
namespace ppapi {

class PPB_Transport_Impl : public Resource {
 public:
  explicit PPB_Transport_Impl(PluginInstance* instance);
  virtual ~PPB_Transport_Impl();

  static const PPB_Transport_Dev* GetInterface();
  virtual PPB_Transport_Impl* AsPPB_Transport_Impl();

  bool Init(const char* name,
            const char* proto);

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Transport_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_TRANSPORT_IMPL_H_

