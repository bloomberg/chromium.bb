/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPINSTANCE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPINSTANCE_H_

#include "native_client/src/shared/npruntime/nacl_npapi.h"

namespace nacl {

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
  // Processes NPP_GetScriptableInstance() invocation from the browser.
  virtual NPObject* GetScriptableInstance() = 0;
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
};

extern bool CheckExecutableVersion(NPP instance, const char *filename);

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPINSTANCE_H_
