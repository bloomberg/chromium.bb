/*
 * Copyright 2008, Google Inc.
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


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/npinstance.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

// Stub class declarations to avoid excessive includes.
namespace nacl_srpc {
class Plugin;
class SharedMemory;
}  // namespace nacl_srpc

namespace nacl {

class VideoMap;

class SRPC_Plugin : public NPInstance, public PortablePluginInterface {
 public:
  explicit SRPC_Plugin(NPP npp, int argc, char* argn[], char* argv[]);

  ~SRPC_Plugin();

  NPError Destroy(NPSavedData** save);
  NPError SetWindow(NPWindow* window);
  NPError GetValue(NPPVariable variable, void *value);
  int16_t HandleEvent(void* event);
  NPObject* GetScriptableInstance();
  NPError NewStream(NPMIMEType type,
                    NPStream* stream,
                    NPBool seekable,
                    uint16_t* stype);
  void StreamAsFile(NPStream* stream, const char* filename);
  NPError DestroyStream(NPStream *stream, NPError reason);
  void URLNotify(const char* url, NPReason reason, void* notify_data);
  NPP GetNPP() { return npp_; }

  virtual nacl_srpc::PluginIdentifier GetPluginIdentifier() { return GetNPP(); }
  virtual VideoMap* video() { return video_; }
  nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>* plugin() { return plugin_; }

 private:
  NPP npp_;
  nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>* plugin_;
  VideoMap* video_;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_H_
