// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/webkit_forwarding.h"

#include "base/compiler_specific.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace ppapi {
struct Preferences;
}

namespace webkit {
namespace ppapi {

class WebKitForwardingImpl : public ::ppapi::WebKitForwarding {
 public:
  WEBKIT_PLUGINS_EXPORT WebKitForwardingImpl();
  WEBKIT_PLUGINS_EXPORT virtual ~WebKitForwardingImpl();

  virtual void CreateFontForwarding(base::WaitableEvent* event,
                                    const PP_FontDescription_Dev& desc,
                                    const std::string& desc_face,
                                    const ::ppapi::Preferences& prefs,
                                    Font** result) OVERRIDE;
};

}  // namespace ppapi
}  // namespace webkit
