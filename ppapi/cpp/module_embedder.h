// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MODULE_EMBEDDER_H_
#define PPAPI_CPP_MODULE_EMBEDDER_H_

namespace pp {

class Module;
// Implemented by the embedder.
//
// Creates the pp::Module object associated with this plugin. Returns the
// module if it was successfully created, or NULL on failure. Upon failure,
// the plugin will be unloaded.
pp::Module* CreateModule();

}  // namespace pp

#endif  // PPAPI_CPP_MODULE_EMBEDDER_H_
