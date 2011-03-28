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


// This file implements the entry points for the windowless O3D plugin that
// relies on a GPU plugin for output.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/display_window_cb.h"
#include "gpu_plugin/command_buffer.h"
#include "gpu_plugin/np_utils/np_browser_stub.h"
#include "gpu_plugin/np_utils/np_object_pointer.h"
#include "gpu_plugin/np_utils/np_utils.h"
#include "plugin/cross/main.h"

using glue::_o3d::PluginObject;
using glue::StreamManager;
using o3d::Event;
using gpu_plugin::NPObjectPointer;
using gpu_plugin::NPInvoke;

namespace {
const uint32 kTimerInterval = 16;

gpu_plugin::NPBrowser* g_browser;

#if defined(OS_WIN)
const wchar_t* const kLogFile = L"debug.log";
#else
const char* const kLogFile = "debug.log";
#endif
}  // end anonymous namespace

#if defined(O3D_INTERNAL_PLUGIN)
namespace o3d {
#else
extern "C" {
#endif

NPError EXPORT_SYMBOL OSCALL NP_Initialize(NPNetscapeFuncs *browserFuncs) {
  CommandLine::Init(0, NULL);
  InitLogging(kLogFile,
              logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
              logging::DONT_LOCK_LOG_FILE,
              logging::APPEND_TO_OLD_LOG_FILE);

  NPError retval = InitializeNPNApi(browserFuncs);
  if (retval != NPERR_NO_ERROR) return retval;

  g_browser = new gpu_plugin::NPBrowser(browserFuncs);

  return NPERR_NO_ERROR;
}

NPError EXPORT_SYMBOL OSCALL NP_Shutdown(void) {
  DLOG(INFO) << "NP_Shutdown";

  CommandLine::Reset();

  return NPERR_NO_ERROR;
}

}  // namespace o3d / extern "C"

namespace o3d {

NPError PlatformNPPGetValue(NPP instance, NPPVariable variable, void *value) {
  return NPERR_NO_ERROR;
}

void OnTimer(NPP instance, uint32 timer_id) {
  PluginObject* plugin_object = static_cast<PluginObject*>(instance->pdata);
  if (plugin_object) {
    // If the GPU plugin object has been set and the renderer is not initialized
    // then attempt to initialize it.
    NPObjectPointer<NPObject> gpu_plugin_object(
        plugin_object->GetGPUPluginObject());
    if (gpu_plugin_object.Get() && !plugin_object->renderer()) {
      NPObjectPointer<NPObject> command_buffer;
      if (NPInvoke(plugin_object->npp(),
                   gpu_plugin_object,
                   "openCommandBuffer",
                   &command_buffer)) {
        DisplayWindowCB default_display;
        default_display.set_npp(plugin_object->npp());
        default_display.set_command_buffer(command_buffer);
        plugin_object->CreateRenderer(default_display);

        // Get the GPU plugins size and resize the renderer.
        int32 width;
        int32 height;
        if (NPInvoke(plugin_object->npp(),
                     gpu_plugin_object,
                     "getWidth",
                     &width) &&
            NPInvoke(plugin_object->npp(),
                     gpu_plugin_object,
                     "getHeight",
                     &height)) {
          plugin_object->renderer()->Resize(width, height);
          plugin_object->client()->Init();
        }
      }
    }

    plugin_object->client()->Tick();
    if (plugin_object->client()->render_mode() ==
        o3d::Client::RENDERMODE_CONTINUOUS) {
      plugin_object->client()->RenderClient(true);
    }
  }
}

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                char *argn[], char *argv[], NPSavedData *saved) {
  NPError error = NPN_SetValue(
      instance, NPPVpluginWindowBool, reinterpret_cast<void*>(false));
  if (error != NPERR_NO_ERROR)
    return error;

  PluginObject* plugin_object = glue::_o3d::PluginObject::Create(
      instance);
  instance->pdata = plugin_object;
  glue::_o3d::InitializeGlue(instance);
  plugin_object->Init(argc, argn, argv);

  gpu_plugin::NPBrowser::get()->ScheduleTimer(instance,
                                              kTimerInterval,
                                              true,
                                              OnTimer);

  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData **save) {
  PluginObject *plugin_object = static_cast<PluginObject*>(instance->pdata);
  if (plugin_object) {
    plugin_object->TearDown();
    NPN_ReleaseObject(plugin_object);
    instance->pdata = NULL;
  }

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow *window) {
  return NPERR_NO_ERROR;
}

// Called when the browser has finished attempting to stream data to
// a file as requested. If fname == NULL the attempt was not successful.
void NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname) {
  PluginObject *plugin_object = static_cast<PluginObject*>(instance->pdata);
  StreamManager *stream_manager = plugin_object->stream_manager();
  stream_manager->SetStreamFile(stream, fname);
}

int16 NPP_HandleEvent(NPP instance, void *event) {
  return 0;
}
}  // namespace o3d

namespace glue {
namespace _o3d {
bool PluginObject::GetDisplayMode(int mode_id, o3d::DisplayMode *mode) {
  return renderer()->GetDisplayMode(mode_id, mode);
}

// TODO: Where should this really live?  It's platform-specific, but in
// PluginObject, which mainly lives in cross/o3d_glue.h+cc.
bool PluginObject::RequestFullscreenDisplay() {
  return false;
}

void PluginObject::CancelFullscreenDisplay() {
}
}  // namespace _o3d
}  // namespace glue
