/*
 * Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "remoting/client/pepper/pepper_plugin.h"

namespace pepper {

PepperPlugin::PepperPlugin(NPNetscapeFuncs* browser_funcs, NPP instance)
    : browser_funcs_(browser_funcs),
      extensions_(NULL),
      instance_(instance) {
  browser_funcs_->getvalue(instance_, NPNVPepperExtensions,
                           static_cast<void*>(&extensions_));
}

PepperPlugin::~PepperPlugin() {
}

}  // namespace pepper
