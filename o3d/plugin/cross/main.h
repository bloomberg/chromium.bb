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


// This header is used by the platform-specific portions of the plugin
// main implementation to define the cross-platform parts of the
// interface and global variables.

#ifndef O3D_PLUGIN_CROSS_MAIN_H_
#define O3D_PLUGIN_CROSS_MAIN_H_

#include <npfunctions.h>

#include "plugin/cross/o3d_glue.h"
#include "plugin/cross/plugin_logging.h"
#include "plugin/cross/stream_manager.h"
#include "third_party/nixysa/static_glue/npapi/common.h"
#include "third_party/nixysa/static_glue/npapi/npn_api.h"

#if defined(OS_LINUX) && !defined(O3D_INTERNAL_PLUGIN)
#define EXPORT_SYMBOL __attribute__((visibility ("default")))
#else
#define EXPORT_SYMBOL
#endif

#if defined(O3D_INTERNAL_PLUGIN)
#define HANDLE_CRASHES void(0)
#else  // O3D_INTERNAL_PLUGIN

#if defined(OS_WIN) || defined(OS_MACOSX)
extern o3d::PluginLogging *g_logger;
#endif

// BreakpadEnabler is a simple class to keep track of whether or not
// we're executing code that we want to handle crashes for
// (when the o3d plugin is running in Firefox, we don't want to handle
// crashes for the Flash plugin or Firefox, just the o3d code)
// Create a stack-based instance at the start of each function
// where crash handling is desired.

#define HANDLE_CRASHES BreakpadEnabler enabler

class BreakpadEnabler {
 public:
  BreakpadEnabler() {
    ++scope_count_;
  }

  virtual ~BreakpadEnabler() {
    --scope_count_;
  }

  static bool IsEnabled() { return scope_count_ > 0; }

 private:
  static int scope_count_;
};

#endif  // O3D_INTERNAL_PLUGIN

#if defined(O3D_INTERNAL_PLUGIN)
namespace o3d {
#else
extern "C" {
#endif
  NPError EXPORT_SYMBOL OSCALL NP_Initialize(NPNetscapeFuncs *browserFuncs
#ifdef OS_LINUX
                                             ,
                                             NPPluginFuncs *pluginFuncs
#endif
                                             );

  NPError EXPORT_SYMBOL OSCALL NP_Shutdown(void);
  NPError EXPORT_SYMBOL OSCALL NP_GetEntryPoints(NPPluginFuncs *pluginFuncs);
}

namespace o3d {

// Plugin entry points, implemented in main.cc.

NPError NPP_Destroy(NPP instance, NPSavedData **save);
NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason);
NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value);

NPError NPP_New(NPMIMEType pluginType,
                NPP instance,
                uint16 mode,
                int16 argc,
                char *argn[],
                char *argv[],
                NPSavedData *saved);

NPError NPP_NewStream(NPP instance,
                      NPMIMEType type,
                      NPStream *stream,
                      NPBool seekable,
                      uint16 *stype);

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value);
NPError NPP_SetWindow(NPP instance, NPWindow *window);

int32 NPP_Write(NPP instance,
                NPStream *stream,
                int32 offset,
                int32 len,
                void *buffer);

int32 NPP_WriteReady(NPP instance, NPStream *stream);
void NPP_Print(NPP instance, NPPrint *platformPrint);
int16 NPP_HandleEvent(NPP instance, void *event);

void NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname);

void NPP_URLNotify(NPP instance,
                   const char *url,
                   NPReason reason,
                   void *notifyData);

// Platform-specific helpers, implemented in main_<platform>.(cc|mm)

NPError PlatformPreNPInitialize();
NPError PlatformPostNPInitialize();
NPError PlatformPreNPShutdown();
NPError PlatformPostNPShutdown();

NPError PlatformNPPDestroy(NPP instance, glue::_o3d::PluginObject *obj);
NPError PlatformNPPGetValue(glue::_o3d::PluginObject *obj,
                            NPPVariable variable,
                            void *value);
int16 PlatformNPPHandleEvent(NPP instance,
                             glue::_o3d::PluginObject *obj,
                             void *event);
NPError PlatformNPPNew(NPP instance, glue::_o3d::PluginObject *obj);
NPError PlatformNPPSetWindow(NPP instance,
                             glue::_o3d::PluginObject *obj,
                             NPWindow *window);
void PlatformNPPStreamAsFile(glue::StreamManager *stream_manager,
                             NPStream *stream,
                             const char *fname);

};  // namespace o3d

#endif  // O3D_PLUGIN_CROSS_MAIN_H_
