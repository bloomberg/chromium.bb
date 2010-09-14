// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_TRANSPORT_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_TRANSPORT_H_

#include "base/scoped_ptr.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_resource.h"

struct PPB_Transport_Dev;

namespace pepper {

class Transport : public Resource {
 public:
  explicit Transport(PluginModule* module);
  virtual ~Transport();
  static const PPB_Transport_Dev* GetInterface();
  virtual Transport* AsTransport() {
    return this;
  }
  bool Init(const char* name,
            const char* proto);
 private:

  DISALLOW_COPY_AND_ASSIGN(Transport);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_TRANSPORT_H_

