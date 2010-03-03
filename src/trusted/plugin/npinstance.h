/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPINSTANCE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPINSTANCE_H_

#include "native_client/src/shared/npruntime/nacl_npapi.h"

namespace nacl {
// Incomplete class declarations.
class NPModule;

// Pure abstract base class for the NaCl MIME type implementation classes.
class NPInstance {
 public:
  virtual ~NPInstance() {}

  // Processes NPP_Destroy() invocation from the browser.
  virtual NPError Destroy(NPSavedData** save) = 0;
  // Processes NPP_SetWindow() invocation from the browser.
  virtual NPError SetWindow(NPWindow* window) = 0;
  // Processes NPP_GetValue() invocation from the browser.
  virtual NPError GetValue(NPPVariable variable, void *value) = 0;
  // Processes NPP_HandleEvent() invocation from the browser.
  virtual int16_t HandleEvent(void* event) = 0;
  // Processes NPP_NewStream() invocation from the browser.
  virtual NPError NewStream(NPMIMEType type,
                            NPStream* stream, NPBool seekable,
                            uint16_t* stype) = 0;
  // Processes NPP_StreamAsFile() invocation from the browser.
  // The filename is given in the POSIX form.
  //
  // Note even though Safari under OS X passes the filename in the classic Mac
  // pathname form like like "MachintoshHD:private:..." to the plugin, the NaCl
  // plugin converts it to the POSIX form before calling StreamAsFile().
  virtual void StreamAsFile(NPStream* stream, const char* filename) = 0;

  // Processes NPP_WriteReady invocation from the browser.
  virtual int32_t WriteReady(NPStream* stream) = 0;

  // Processes NPP_Write invocation from the browser.
  virtual int32_t Write(NPStream* stream,
                        int32_t offset,
                        int32_t len,
                        void* buf) = 0;

  // Processes NPP_DestroyStream() invocation from the browser.
  virtual NPError DestroyStream(NPStream *stream, NPError reason) = 0;
  // Processes NPP_URLNotify() invocation from the browser.
  virtual void URLNotify(const char* url, NPReason reason,
                         void* notify_data) = 0;

  // NPModule responsible for implementing connections to NaCl modules.
  virtual NPModule* module() = 0;
  virtual void set_module(NPModule* module) = 0;
};

extern bool CheckExecutableVersion(NPP instance, const char *filename);

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPINSTANCE_H_
