// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PEPPER_PEPPER_PLUGIN_H_
#define REMOTING_CLIENT_PEPPER_PEPPER_PLUGIN_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace pepper {

class PepperPlugin {
 public:
  // This class stores information about the plugin that cannot be instantiated
  // as part of the PepperPlugin class because it is required before the
  // PepperPlugin has been created.
  class Info {
   public:
    // True if these fields have been initialized.
    bool initialized;

    // MIME type and description.
    const char* mime_description;

    // Name of plugin (shown in about:plugins).
    const char* plugin_name;

    // Short description of plugin (shown in about:plugins).
    const char* plugin_description;
  };

  PepperPlugin(NPNetscapeFuncs* browser_funcs, NPP instance);
  virtual ~PepperPlugin();

  NPNetscapeFuncs* browser() const { return browser_funcs_; }
  NPNExtensions* extensions() const { return extensions_; }
  NPP instance() const { return instance_; }

  // Virtual methods to be implemented by the plugin subclass.

  virtual NPError New(NPMIMEType pluginType, int16 argc,
                      char* argn[], char* argv[]) {
    return NPERR_GENERIC_ERROR;
  }

  virtual NPError Destroy(NPSavedData** save) {
    return NPERR_GENERIC_ERROR;
  }

  virtual NPError SetWindow(NPWindow* window) {
    return NPERR_GENERIC_ERROR;
  }

  virtual NPError NewStream(NPMIMEType type, NPStream* stream,
                            NPBool seekable, uint16* stype) {
    return NPERR_GENERIC_ERROR;
  }

  virtual NPError DestroyStream(NPStream* stream, NPReason reason) {
    return NPERR_GENERIC_ERROR;
  }

  virtual void StreamAsFile(NPStream* stream, const char* fname) {
  }

  virtual int32 WriteReady(NPStream* stream) {
    return 0;
  }

  virtual int32 Write(NPStream* stream, int32 offset, int32 len, void* buffer) {
    return -1;
  }

  virtual void Print(NPPrint* platformPrint) {
  }

  virtual int16 HandleEvent(void* event) {
    return false;
  }

  virtual void URLNotify(const char* url, NPReason reason, void* nofifyData) {
  }

  virtual NPError GetValue(NPPVariable variable, void* value) {
    return NPERR_GENERIC_ERROR;
  }

  virtual NPError SetValue(NPNVariable variable, void* value) {
    return NPERR_GENERIC_ERROR;
  }

 private:
  // Browser callbacks.
  NPNetscapeFuncs* browser_funcs_;
  NPNExtensions* extensions_;

  NPP instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlugin);
};

}  // namespace pepper

#endif  // REMOTING_CLIENT_PEPPER_PEPPER_PLUGIN_H_
