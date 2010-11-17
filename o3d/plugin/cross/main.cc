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


#include "plugin/cross/main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "plugin/cross/config.h"
#include "plugin/cross/out_of_memory.h"
#include "plugin/cross/whitelist.h"
#ifdef OS_WIN
#include "breakpad/win/bluescreen_detector.h"
#endif

using glue::_o3d::PluginObject;
using glue::StreamManager;

#if !defined(O3D_INTERNAL_PLUGIN)

#ifdef OS_WIN
#define O3D_DEBUG_LOG_FILENAME L"debug.log"
#else
#define O3D_DEBUG_LOG_FILENAME "debug.log"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
o3d::PluginLogging *g_logger = NULL;
static bool g_logging_initialized = false;
#ifdef OS_WIN
static o3d::BluescreenDetector *g_bluescreen_detector = NULL;
#endif  // OS_WIN
#endif  // OS_WIN || OS_MACOSX

// We would normally make this a stack variable in main(), but in a
// plugin, that's not possible, so we make it a global. When the DLL is loaded
// this it gets constructed and when it is unlooaded it is destructed. Note
// that this cannot be done in NP_Initialize and NP_Shutdown because those
// calls do not necessarily signify the DLL being loaded and unloaded. If the
// DLL is not unloaded then the values of global variables are preserved.
static base::AtExitManager g_at_exit_manager;

int BreakpadEnabler::scope_count_ = 0;

#endif  // O3D_INTERNAL_PLUGIN

namespace o3d {

NPError NP_GetValue(NPPVariable variable, void *value) {
  switch (variable) {
    case NPPVpluginNameString:
      *static_cast<char **>(value) = const_cast<char*>(O3D_PLUGIN_NAME);
      break;
    case NPPVpluginDescriptionString:
      *static_cast<char **>(value) = const_cast<char*>(O3D_PLUGIN_DESCRIPTION);
      break;
    default:
      return NPERR_INVALID_PARAM;
      break;
  }
  return NPERR_NO_ERROR;
}

int16 NPP_HandleEvent(NPP instance, void *event) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return 0;
  }

  return PlatformNPPHandleEvent(instance, obj, event);
}

NPError NPP_NewStream(NPP instance,
                      NPMIMEType type,
                      NPStream *stream,
                      NPBool seekable,
                      uint16 *stype) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return NPERR_INVALID_PARAM;
  }

  StreamManager *stream_manager = obj->stream_manager();
  if (stream_manager->NewStream(stream, stype)) {
    return NPERR_NO_ERROR;
  } else {
    // TODO: find out which error we should return
    return NPERR_INVALID_PARAM;
  }
}

NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return NPERR_NO_ERROR;
  }

  StreamManager *stream_manager = obj->stream_manager();
  if (stream_manager->DestroyStream(stream, reason)) {
    return NPERR_NO_ERROR;
  } else {
    // TODO: find out which error we should return
    return NPERR_INVALID_PARAM;
  }
}

int32 NPP_WriteReady(NPP instance, NPStream *stream) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return 0;
  }

  StreamManager *stream_manager = obj->stream_manager();
  return stream_manager->WriteReady(stream);
}

int32 NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len,
                void *buffer) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return 0;
  }

  StreamManager *stream_manager = obj->stream_manager();
  return stream_manager->Write(stream, offset, len, buffer);
}

void NPP_Print(NPP instance, NPPrint *platformPrint) {
  HANDLE_CRASHES;
}

void NPP_URLNotify(NPP instance, const char *url, NPReason reason,
                   void *notifyData) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return;
  }

  StreamManager *stream_manager = obj->stream_manager();
  stream_manager->URLNotify(url, reason, notifyData);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return NPERR_INVALID_PARAM;
  }

  switch (variable) {
    case NPPVpluginScriptableNPObject: {
      void **v = static_cast<void **>(value);
      // Return value is expected to be retained
      GLUE_PROFILE_START(instance, "retainobject");
      NPN_RetainObject(obj);
      GLUE_PROFILE_STOP(instance, "retainobject");
      *v = obj;
      break;
    }
    default: {
      NPError ret = PlatformNPPGetValue(obj, variable, value);
      if (ret == NPERR_INVALID_PARAM)
        ret = o3d::NP_GetValue(variable, value);
      return ret;
    }
  }
  return NPERR_NO_ERROR;
}

NPError NPP_New(NPMIMEType pluginType,
                NPP instance,
                uint16 mode,
                int16 argc,
                char *argn[],
                char *argv[],
                NPSavedData *saved) {
  HANDLE_CRASHES;

#if !defined(O3D_INTERNAL_PLUGIN) && (defined(OS_WIN) || defined(OS_MACOSX))
  // TODO(tschmelcher): Support this on Linux?
  if (!g_logging_initialized) {
    // Get user config metrics. These won't be stored though unless the user
    // opts-in for usagestats logging
    GetUserAgentMetrics(instance);
    GetUserConfigMetrics();
    // Create usage stats logs object
    g_logger = o3d::PluginLogging::InitializeUsageStatsLogging();
#ifdef OS_WIN
    if (g_logger) {
      // Setup blue-screen detection
      g_bluescreen_detector = new o3d::BluescreenDetector();
      g_bluescreen_detector->Start();
    }
#endif
    g_logging_initialized = true;
  }
#endif

  if (!IsDomainAuthorized(instance)) {
    return NPERR_INVALID_URL;
  }

  PluginObject *obj = glue::_o3d::PluginObject::Create(instance);
  instance->pdata = obj;
  glue::_o3d::InitializeGlue(instance);
  obj->Init(argc, argn, argv);
  return PlatformNPPNew(instance, obj);
}

NPError NPP_Destroy(NPP instance, NPSavedData **save) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return NPERR_NO_ERROR;
  }

  NPError err = PlatformNPPDestroy(instance, obj);

  NPN_ReleaseObject(obj);
  instance->pdata = NULL;

  return err;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
  HANDLE_CRASHES;
  return NPERR_GENERIC_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return NPERR_NO_ERROR;
  }

  return PlatformNPPSetWindow(instance, obj, window);
}

// Called when the browser has finished attempting to stream data to
// a file as requested. If fname == NULL the attempt was not successful.
void NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname) {
  HANDLE_CRASHES;
  PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
  if (!obj) {
    return;
  }

  StreamManager *stream_manager = obj->stream_manager();
  PlatformNPPStreamAsFile(stream_manager, stream, fname);
}

}  // namespace o3d

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
                                           ) {
  HANDLE_CRASHES;
  NPError err = InitializeNPNApi(browserFuncs);
  if (err != NPERR_NO_ERROR) {
    return err;
  }

#ifdef OS_LINUX
  NP_GetEntryPoints(pluginFuncs);
#endif  // OS_LINUX

#if !defined(O3D_INTERNAL_PLUGIN)
  if (!o3d::SetupOutOfMemoryHandler())
    return NPERR_MODULE_LOAD_FAILED_ERROR;
#endif  // O3D_INTERNAL_PLUGIN

  err = o3d::PlatformPreNPInitialize();
  if (err != NPERR_NO_ERROR) {
    return err;
  }

#if !defined(O3D_INTERNAL_PLUGIN)
  // Turn on the logging.
  CommandLine::Init(0, NULL);

  FilePath log;
  file_util::GetTempDir(&log);
  log.Append(O3D_DEBUG_LOG_FILENAME);

  InitLogging(log.value().c_str(),
              logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
              logging::DONT_LOCK_LOG_FILE,
              logging::APPEND_TO_OLD_LOG_FILE);
#endif  // O3D_INTERNAL_PLUGIN

  DLOG(INFO) << "NP_Initialize";

  return o3d::PlatformPostNPInitialize();
}

NPError EXPORT_SYMBOL OSCALL NP_Shutdown(void) {
  HANDLE_CRASHES;
  DLOG(INFO) << "NP_Shutdown";

  NPError err = o3d::PlatformPreNPShutdown();
  if (err != NPERR_NO_ERROR) {
    return err;
  }

#if !defined(O3D_INTERNAL_PLUGIN)
#if defined(OS_WIN) || defined(OS_MACOSX)
  if (g_logger) {
    // Do a last sweep to aggregate metrics before we shut down
    g_logger->ProcessMetrics(true, false, false);
    delete g_logger;
    g_logger = NULL;
    g_logging_initialized = false;
    stats_report::g_global_metrics.Uninitialize();
  }
#endif  // OS_WIN || OS_MACOSX

  CommandLine::Reset();

#ifdef OS_WIN
  // Strictly speaking, on windows, it's not really necessary to call
  // Stop(), but we do so for completeness
  if (g_bluescreen_detector) {
    g_bluescreen_detector->Stop();
    delete g_bluescreen_detector;
    g_bluescreen_detector = NULL;
  }
#endif  // OS_WIN
#endif  // O3D_INTERNAL_PLUGIN

  return o3d::PlatformPostNPShutdown();
}

NPError EXPORT_SYMBOL OSCALL NP_GetEntryPoints(NPPluginFuncs *pluginFuncs) {
  HANDLE_CRASHES;
  pluginFuncs->version = 11;
  pluginFuncs->size = sizeof(*pluginFuncs);
  pluginFuncs->newp = o3d::NPP_New;
  pluginFuncs->destroy = o3d::NPP_Destroy;
  pluginFuncs->setwindow = o3d::NPP_SetWindow;
  pluginFuncs->newstream = o3d::NPP_NewStream;
  pluginFuncs->destroystream = o3d::NPP_DestroyStream;
  pluginFuncs->asfile = o3d::NPP_StreamAsFile;
  pluginFuncs->writeready = o3d::NPP_WriteReady;
  pluginFuncs->write = o3d::NPP_Write;
  pluginFuncs->print = o3d::NPP_Print;
  pluginFuncs->event = o3d::NPP_HandleEvent;
  pluginFuncs->urlnotify = o3d::NPP_URLNotify;
  pluginFuncs->getvalue = o3d::NPP_GetValue;
  pluginFuncs->setvalue = o3d::NPP_SetValue;

  return NPERR_NO_ERROR;
}

char* NP_GetMIMEDescription(void) {
  return const_cast<char*>(O3D_PLUGIN_NPAPI_MIMETYPE "::O3D MIME");
}

}  // namespace o3d / extern "C"

#if !defined(O3D_INTERNAL_PLUGIN)
extern "C" {
NPError EXPORT_SYMBOL NP_GetValue(void *instance, NPPVariable variable,
                                  void *value) {
  return o3d::NP_GetValue(variable, value);
}
}
#endif
