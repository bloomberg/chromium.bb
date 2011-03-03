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

#include <build/build_config.h>
#ifdef OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif  // OS_WIN

#include <string>
#include <algorithm>
#include "core/cross/image_utils.h"
#include "core/cross/renderer.h"
#include "core/cross/texture.h"
#include "core/cross/client_info.h"
#include "plugin/cross/o3d_glue.h"
#include "plugin/cross/config.h"
#include "plugin/cross/stream_manager.h"
#include "third_party/nixysa/static_glue/npapi/npn_api.h"
#include "client_glue.h"
#include "globals_glue.h"

#ifdef OS_MACOSX
#include "plugin_mac.h"
#endif

#ifdef OS_LINUX
#include <X11/cursorfont.h>
#endif

namespace glue {
namespace _o3d {

using o3d::Bitmap;
using o3d::Texture;
using o3d::Texture2D;

void RegisterType(NPP npp, const ObjectBase::Class *clientclass,
                  NPClass *npclass) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  plugin_object->RegisterType(clientclass, npclass);
}

bool CheckObject(NPP npp, NPObject *npobject,
                 const ObjectBase::Class *clientclass) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  return plugin_object->CheckObject(npobject, clientclass);
}

NPAPIObject *GetNPObject(NPP npp, ObjectBase *object) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  return plugin_object->GetNPObject(object);
}

NPObject *Allocate(NPP npp, NPClass *npclass) {
  return new NPAPIObject(npp);
}
void Deallocate(NPObject *object) {
  NPAPIObject *npobject = static_cast<NPAPIObject *>(object);
  if (npobject->mapped()) {
    NPP npp = npobject->npp();
    PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
    plugin_object->UnmapObject(npobject);
  }
  delete npobject;
}

ServiceLocator *GetServiceLocator(NPP npp) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  return plugin_object->service_locator();
}

Client *GetClient(NPP npp) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  return plugin_object->client();
}

PluginObject::PluginObject(NPP npp)
    : npp_(npp),
      evaluation_counter_(&service_locator_),
      class_manager_(&service_locator_),
      client_info_manager_(&service_locator_),
      object_manager_(&service_locator_),
      profiler_(&service_locator_),
      main_thread_task_poster_(&service_locator_, npp),
      fullscreen_(false),
      renderer_(NULL),
      features_(NULL),
      fullscreen_region_valid_(false),
      renderer_init_status_(Renderer::UNINITIALIZED),
      pending_ticks_(0),
#ifdef OS_WIN
      hWnd_(NULL),
      plugin_hWnd_(NULL),
      content_hWnd_(NULL),
      got_dblclick_(false),
      painted_once_(false),
#endif
#ifdef OS_MACOSX
      renderer_is_software_(false),
      scroll_is_in_progress_(false),
      drawing_model_(NPDrawingModelQuickDraw),
      event_model_(NPEventModelCarbon),
      mac_window_(0),
      mac_window_selected_tab_(0),
      mac_cocoa_window_(0),
      mac_surface_hidden_(0),
      mac_cg_context_ref_(0),
      mac_agl_context_(0),
      mac_cgl_context_(0),
      mac_cgl_pbuffer_(0),
      last_mac_event_time_(0),
      gl_layer_(NULL),
      mac_fullscreen_window_(NULL),
      mac_fullscreen_nsopenglcontext_(NULL),
      mac_fullscreen_nsopenglpixelformat_(NULL),
      was_offscreen_(false),
#endif  // OS_MACOSX
#ifdef OS_LINUX
      display_(NULL),
      window_(0),
      fullscreen_window_(0),
      xt_widget_(NULL),
      xt_app_context_(NULL),
      xt_interval_(0),
      last_click_time_(0),
      drawable_(0),
      gdk_display_(NULL),
      gtk_container_(NULL),
      gtk_fullscreen_container_(NULL),
      gtk_event_source_(NULL),
      event_handler_id_(0),
      timeout_id_(0),
      fullscreen_pending_(false),
      draw_(true),
      in_plugin_(false),
#endif
#if defined(CB_SERVICE_REMOTE)
      gpu_plugin_object_(NULL),
#endif
      np_v8_bridge_(&service_locator_, npp),
      stream_manager_(new StreamManager(npp)),
      cursor_type_(o3d::Cursor::DEFAULT),
      prev_width_(0),
      prev_height_(0),
      offscreen_rendering_enabled_(false) {
#if defined(OS_WIN) || defined(OS_LINUX)
  memset(cursors_, 0, sizeof(cursors_));
#endif

#ifdef OS_MACOSX
  memset(last_buffer_rect_, 0, sizeof(last_buffer_rect_));
#endif

#ifdef OS_LINUX
  memset(got_double_click_, 0, sizeof(got_double_click_));
#endif

  // create an O3D object
  client_ = new Client(&service_locator_);

  globals_npobject_ = glue::CreateStaticNPObject(npp);
  client_npobject_ =
      glue::namespace_o3d::class_Client::GetNPObject(npp, client_);
  user_agent_ = o3d::GetUserAgent(npp);
}

PluginObject::~PluginObject() {
}

void PluginObject::Init(int argc, char* argn[], char* argv[]) {
  DCHECK(argc == 0 || (argn != NULL && argv != NULL));
  features_ = new Features(&service_locator_);

  std::string o3d_name("o3d_features");

  for (int ii = 0; ii < argc; ++ii) {
    DCHECK(argn[ii]);
    const char* name = argn[ii];
    if (o3d_name.compare(name) == 0) {
      DCHECK(argv[ii]);
      features_->Init(argv[ii]);
      break;
    }
  }

  NPObject* np_window;
  NPN_GetValue(npp_, NPNVWindowNPObject, &np_window);
  o3d::NPObjectPtr<NPObject> np_window_ptr =
      o3d::NPObjectPtr<NPObject>::AttachToReturned(np_window);
  np_v8_bridge_.Initialize(np_window_ptr);

  o3d::NPObjectPtr<NPObject> np_plugin_ptr(this);
  np_v8_bridge_.SetGlobalProperty(o3d::String("plugin"),
                                  np_plugin_ptr);
}

void PluginObject::TearDown() {
  DisableOffscreenRendering();
#ifdef OS_WIN
  ClearPluginProperty(hWnd_);
#elif defined(OS_MACOSX)
  o3d::ReleaseSafariBrowserWindow(mac_cocoa_window_);
  CleanupFullscreenOpenGLContext();
#elif defined(OS_LINUX)
  SetDisplay(NULL);
#endif  // OS_WIN
  UnmapAll();

  // Delete the StreamManager to cleanup any streams that are in midflight.
  // This needs to happen here, before the client is deleted as the streams
  // could be holding references to FileRequest objects.
  stream_manager_.reset(NULL);

  delete client_;
  client_ = NULL;

  // Release the graphics context before deletion.
  DeleteRenderer();

  delete features_;
  features_ = NULL;

  // There is a reference cycle between the V8 bridge and the plugin.
  // Explicitly remove all V8 references during tear-down, so that the cycle is
  // broken, and the reference counting system will successfully delete the
  // plugin.
  np_v8_bridge_.ReleaseNPObjects();
}

void PluginObject::CreateRenderer(const o3d::DisplayWindow& display_window) {
  // In case CreateRenderer is called more than once, reset to the
  // uninitialized state..
  DeleteRenderer();
  renderer_init_status_ = o3d::Renderer::UNINITIALIZED;

  if (features_->render_modes() != Renderer::RENDER_MODE_2D) {
    if (!o3d::CheckConfig(npp_)) {
      renderer_init_status_ = o3d::Renderer::GPU_NOT_UP_TO_SPEC;
    } else {
      renderer_ = o3d::Renderer::CreateDefaultRenderer(&service_locator_);
      DCHECK(renderer_);

      // Attempt to initialize renderer.
      renderer_init_status_ = renderer_->Init(display_window, false);
      if (renderer_init_status_ != o3d::Renderer::SUCCESS) {
        DeleteRenderer();
      }
    }
#if !defined(SUPPORT_CAIRO)
  } else {
    // The caller requested 2D only mode, but this platform does not support 2d
    renderer_init_status_ = o3d::Renderer::INITIALIZATION_ERROR;
  }
#else
  }
  if ((renderer_init_status_ != o3d::Renderer::SUCCESS) &&
      (features_->render_modes() != Renderer::RENDER_MODE_3D)) {
    // Attempt to fall back to o2d renderer
    renderer_ = o3d::Renderer::Create2DRenderer(&service_locator_);
    if (renderer_) {
      renderer_init_status_ = renderer_->Init(display_window, false);
      if (renderer_init_status_ != o3d::Renderer::SUCCESS) {
        DeleteRenderer();
      } else {
        ClientInfoManager* client_info_manager =
              service_locator()->GetService<ClientInfoManager>();
        client_info_manager->SetRender2d(true);
      }
    }
  }
#endif
}

void PluginObject::DeleteRenderer() {
  if (renderer_) {
    delete renderer_;
    renderer_ = NULL;
  }
}


#ifdef OS_MACOSX

// These following 3 methods are needed for a Safari workaround - basically it
// does not notify us when our tab is hidden by tab switching, and just stops
// sending us events.  The workaround is to keep track of when the browser last
// sent us an event, and what the selected tab was at that time. If we find that
// we are no longer getting events and the selected tab value has changed,
// DetectTabHiding() starts returning true when the timer calls it and code
// elsewhere can take action.  SafariBrowserWindowForWindowRef() and
// SelectedTabForSafariBrowserWindow() are both written in such a way that
// non-Safari browsers should yield NULL,  so DetectTabHiding would always
// return false and the workaround would not be triggered.


// Function gets notified every time we receive a Mac event.
// It records the time of the event and tries to read the selected tab value
// from Safari (on other browsers this tab value should always be NULL).
// Written so that last_mac_event_time_ is always valid or NULL.
void PluginObject::MacEventReceived(bool updateTab) {
  CFDateRef now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
  CFDateRef previousTime = last_mac_event_time_;
  last_mac_event_time_ = now;
  if (previousTime != NULL) {
    CFRelease(previousTime);
  }
  mac_cocoa_window_ = o3d::SafariBrowserWindowForWindowRef(mac_window_);
  if (!mac_window_selected_tab_ || updateTab) {
    mac_window_selected_tab_ =
        o3d::SelectedTabForSafariBrowserWindow(mac_cocoa_window_);
  }
}

// Returns the time elapsed since the MacEventReceived function was last called.
CFTimeInterval PluginObject::TimeSinceLastMacEvent() {
  CFTimeInterval elapsed = 0.0;
  if (last_mac_event_time_ != NULL) {
    CFDateRef now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    elapsed = CFDateGetTimeIntervalSinceDate(now, last_mac_event_time_);
    CFRelease(now);
  }
  return elapsed;
}

// Detects if Safari has hidden our tab.
// The heuristic is that we have not received any Mac events for a certain time
// and also the value for selected tab is different from the value it had the
// last time we did get a Mac event.
bool PluginObject::DetectTabHiding() {
  const CFTimeInterval kMacTimeOut = 0.2;  // a fifth of a second
  if (TimeSinceLastMacEvent() < kMacTimeOut)
    return false;

  if (!mac_cocoa_window_) {
    mac_cocoa_window_ = o3d::SafariBrowserWindowForWindowRef(mac_window_);
  }

  return o3d::SelectedTabForSafariBrowserWindow(mac_cocoa_window_) !=
      mac_window_selected_tab_;
}


// Pick a constant way out of Apple's 0-22 range for our "no theme cursor"
// constant.
const ThemeCursor kNoThemeCursorForThat = 1000;

// Map o3d cursors to Mac theme cursors if possible, otherwise return
// kNoThemeCursorForThat.
ThemeCursor O3DToMacThemeCursor(o3d::Cursor::CursorType cursor_type) {
  switch (cursor_type) {
    case o3d::Cursor::DEFAULT:
      return kThemeArrowCursor;
    case o3d::Cursor::NONE:  // There is no standard blank cursor.
      return kNoThemeCursorForThat;
    case o3d::Cursor::CROSSHAIR:
      return kThemeCrossCursor;
    case o3d::Cursor::POINTER:
      return kThemePointingHandCursor;
    case o3d::Cursor::E_RESIZE:
      return kThemeResizeRightCursor;
    case o3d::Cursor::NE_RESIZE:  // No diagonal resize directions on Mac.
      return kNoThemeCursorForThat;
    case o3d::Cursor::NW_RESIZE:  // No diagonal resize directions on Mac.
      return kNoThemeCursorForThat;
    case o3d::Cursor::N_RESIZE:
      return kThemeResizeUpCursor;
    case o3d::Cursor::SE_RESIZE:  // No diagonal resize directions on Mac.
      return kNoThemeCursorForThat;
    case o3d::Cursor::SW_RESIZE:  // No diagonal resize directions on Mac.
      return kNoThemeCursorForThat;
    case o3d::Cursor::S_RESIZE:
      return kThemeResizeDownCursor;
    case o3d::Cursor::W_RESIZE:
      return kThemeResizeLeftCursor;
    case o3d::Cursor::MOVE:
      return kThemeOpenHandCursor;
    case o3d::Cursor::TEXT:
      return kThemeIBeamCursor;
    case o3d::Cursor::WAIT:
      return kThemeWatchCursor;
    case o3d::Cursor::PROGRESS:
      return kThemeSpinningCursor;
    case o3d::Cursor::HELP:  // No standard Help cursor.
      return kNoThemeCursorForThat;
  }

  return kNoThemeCursorForThat;
}


// Set cursor to the one specified in  cursor_type_.
// TODO add support for cursors that don't have equivalent Mac
// theme cursors.
void PluginObject::PlatformSpecificSetCursor() {
  if (cursor_type_ == o3d::Cursor::NONE) {
    // hide cursor if visible
    if (CGCursorIsVisible()) {
      CGDisplayHideCursor(kCGDirectMainDisplay);
    }
  } else {
    ThemeCursor the_id = O3DToMacThemeCursor(cursor_type_);

    if (the_id != kNoThemeCursorForThat) {
      SetThemeCursor(the_id);
    } else {
      // could add code here to set other cursors by other means
      SetThemeCursor(kThemeArrowCursor);
    }
    // show cursor if hidden
    if (!CGCursorIsVisible())
      CGDisplayShowCursor(kCGDirectMainDisplay);
  }
}

bool PluginObject::SetRendererIsSoftware(bool state) {
  renderer_is_software_ = state;
  ClientInfoManager* client_info_manager =
      service_locator()->GetService<ClientInfoManager>();
  client_info_manager->SetSoftwareRenderer(state);
}

#endif  // OS_MACOSX

void PluginObject::RegisterType(const ObjectBase::Class *clientclass,
                                NPClass *npclass) {
  client_to_np_class_map_[clientclass] = npclass;
  np_to_client_class_map_[npclass] = clientclass;
}

bool PluginObject::CheckObject(NPObject *npobject,
                               const ObjectBase::Class *clientclass) const {
  if (!npobject) return true;
  NPClass *npclass = npobject->_class;
  NPToClientClassMap::const_iterator it = np_to_client_class_map_.find(npclass);
  if (it == np_to_client_class_map_.end()) return false;
  if (static_cast<NPAPIObject *>(npobject)->npp()->pdata != this) {
    // The object was created by another plug-in instance. Don't allow direct
    // references to these objects, that would cause havok.
    return false;
  }
  return ObjectBase::ClassIsA(it->second, clientclass);
}

NPAPIObject *PluginObject::GetNPObject(ObjectBase *object) {
  if (!object) return NULL;
  NPAPIObject *npobject = object_map_[object->id()];
  if (!npobject) {
    NPClass *npclass = GetNPClass(object->GetClass());
    GLUE_PROFILE_START(npp_, "createobject");
    npobject = static_cast<NPAPIObject *>(NPN_CreateObject(npp_, npclass));
    GLUE_PROFILE_STOP(npp_, "createobject");
    npobject->Initialize(object);
    object_map_[object->id()] = npobject;
    npobject->set_mapped(true);
  } else {
    GLUE_PROFILE_START(npp_, "retainobject");
    NPN_RetainObject(npobject);
    GLUE_PROFILE_STOP(npp_, "retainobject");
  }
  return npobject;
}

void PluginObject::UnmapObject(NPAPIObject *npobject) {
  npobject->set_mapped(false);
  object_map_.erase(npobject->id());
}

void PluginObject::UnmapAll() {
  for (ClientObjectMap::iterator it = object_map_.begin();
       it != object_map_.end(); ++it) {
    it->second->set_mapped(false);
  }
  object_map_.clear();
}

NPClass *PluginObject::GetNPClass(const ObjectBase::Class *clientclass) {
  const ObjectBase::Class *cursor = clientclass;
  while (cursor) {
    ClientToNPClassMap::const_iterator it =
        client_to_np_class_map_.find(cursor);
    if (it != client_to_np_class_map_.end()) {
      NPClass *npclass = it->second;
      if (cursor != clientclass) client_to_np_class_map_[clientclass] = npclass;
      return npclass;
    }
    cursor = cursor->parent();
  }
  return NULL;
}
// Static function to handle log asserts in the FATAL ERROR case
void PluginObject::LogAssertHandlerFunction(const std::string& str) {
  DLOG(ERROR) << "FATAL LOG ERROR: " << str;
}

#if defined(CB_SERVICE_REMOTE)
void PluginObject::SetGPUPluginObject(NPObject* gpu_plugin_object) {
  if (gpu_plugin_object) {
    NPN_RetainObject(gpu_plugin_object);
  }

  if (gpu_plugin_object_) {
    NPN_ReleaseObject(gpu_plugin_object_);
  }

  gpu_plugin_object_ = gpu_plugin_object;
}
#endif

enum {
  kPropClient,
  kPropGpuConfig,
  kNumPropertyIds
};

static NPIdentifier property_ids[kNumPropertyIds];
static const NPUTF8 *property_names[kNumPropertyIds] = {
  "client",
  "gpuConfig",
};

enum {
  kMethodEval,

#if defined(CB_SERVICE_REMOTE)
  kMethodSetGPUPluginObject,
  kMethodGetGPUPluginObject,
#endif

  kNumMethodIds,
};

static NPIdentifier method_ids[kNumMethodIds];
static const NPUTF8 *method_names[kNumMethodIds] = {
  "eval",

#if defined(CB_SERVICE_REMOTE)
  "setGPUPluginObject",
  "getGPUPluginObject",
#endif
};

static NPObject *PluginAllocate(NPP npp, NPClass *npclass) {
  return new PluginObject(npp);
}

static void PluginDeallocate(NPObject *object) {
  delete static_cast<PluginObject *>(object);
}

static bool PluginHasMethod(NPObject *header, NPIdentifier name) {
  DebugScopedId id(name);
  PluginObject *plugin_object = static_cast<PluginObject *>(header);
  for (int i = 0; i < kNumMethodIds; ++i) {
    if (name == method_ids[i]) {
      return true;
    }
  }

  NPObject *globals = plugin_object->globals_npobject();
  return globals->_class->hasMethod(globals, name);
}

static bool PluginInvoke(NPObject *header, NPIdentifier name,
                         const NPVariant *args, uint32_t arg_count,
                         NPVariant *np_result) {
  DebugScopedId id(name);
  PluginObject *plugin_object = static_cast<PluginObject *>(header);
  if (name == method_ids[kMethodEval]) {
    return plugin_object->np_v8_bridge()->Evaluate(args, arg_count, np_result);
  }
#if defined(CB_SERVICE_REMOTE)
  else if (name == method_ids[kMethodGetGPUPluginObject]) {
    if (arg_count != 0)
      return false;
    ValueToNPVariant(plugin_object->GetGPUPluginObject(), np_result);
    return true;
  } else if (name == method_ids[kMethodSetGPUPluginObject]) {
    if (arg_count != 1)
      return false;
    VOID_TO_NPVARIANT(*np_result);
    NPObjectPointer<NPObject> gpu_plugin_object;
    if (NPVariantToValue(&gpu_plugin_object, args[0])) {
      plugin_object->SetGPUPluginObject(gpu_plugin_object.Get());
      return true;
    } else {
      return false;
    }
  }  // NOLINT
#endif  // CB_SERVICE_REMOTE
  else {  // NOLINT
    NPObject *globals = plugin_object->globals_npobject();
    return globals->_class->invoke(globals, name, args, arg_count, np_result);
  }
}

static bool PluginInvokeDefault(NPObject *header, const NPVariant *args,
                                uint32_t arg_count, NPVariant *result) {
  PluginObject *plugin_object = static_cast<PluginObject *>(header);
  NPP npp = plugin_object->npp();
  NPObject *globals = plugin_object->globals_npobject();
  return globals->_class->invokeDefault(globals, args, arg_count, result);
}

static bool PluginHasProperty(NPObject *header, NPIdentifier name) {
  DebugScopedId id(name);
  PluginObject *plugin_object = static_cast<PluginObject *>(header);
  NPP npp = plugin_object->npp();
  for (unsigned int i = 0; i < kNumPropertyIds; ++i) {
    if (name == property_ids[i]) return true;
  }
  NPObject *globals = plugin_object->globals_npobject();
  return globals->_class->hasProperty(globals, name);
}

static bool PluginGetProperty(NPObject *header, NPIdentifier name,
                              NPVariant *variant) {
  DebugScopedId id(name);
  PluginObject *plugin_object = static_cast<PluginObject *>(header);
  NPP npp = plugin_object->npp();
  if (name == property_ids[kPropGpuConfig]) {
    // Gets the GPU config (VendorID, DeviceID, name) as a string.
    // NOTE: this should probably be removed before we ship.
    o3d::GPUDevice device;
    bool result = o3d::GetGPUDevice(npp, &device);
    if (!result) return false;
    std::string return_value = std::string("VendorID = 0x");
    char id_text[9];
    base::snprintf(id_text, sizeof(id_text), "%04x", device.vendor_id);
    return_value += id_text;
    return_value += ", DeviceID = 0x";
    base::snprintf(id_text, sizeof(id_text), "%04x", device.device_id);
    return_value += id_text;
    return_value += ", DeviceName = '";
    return_value += device.name + "'";
    return_value += ", Driver = '";
    return_value += device.driver + "'";
    return_value += ", Description = '";
    return_value += device.description + "'";
    return_value += ", GUID = 0x";
    base::snprintf(id_text, sizeof(id_text), "%08x", device.guid);
    return_value += id_text;
    GLUE_PROFILE_START(npp, "StringToNPVariant");
    bool temp = StringToNPVariant(return_value, variant);
    GLUE_PROFILE_STOP(npp, "StringToNPVariant");
    return temp;
  }

  if (name == property_ids[kPropClient]) {
    NPObject *npobject = plugin_object->client_npobject();
    GLUE_PROFILE_START(npp, "retainobject");
    NPN_RetainObject(npobject);
    GLUE_PROFILE_STOP(npp, "retainobject");
    OBJECT_TO_NPVARIANT(npobject, *variant);
    return true;
  }
  NPObject *globals = plugin_object->globals_npobject();
  return globals->_class->getProperty(globals, name, variant);
}

static bool PluginSetProperty(NPObject *header, NPIdentifier name,
                              const NPVariant *variant) {
  DebugScopedId id(name);
  PluginObject *plugin_object = static_cast<PluginObject *>(header);
  NPP npp = plugin_object->npp();
  if (name == property_ids[kPropClient]) {
    return false;
  }
  NPObject *globals = plugin_object->globals_npobject();
  return globals->_class->setProperty(globals, name, variant);
}

static bool PluginEnumerate(NPObject *header, NPIdentifier **value,
                            uint32_t *count) {
  *count = kNumPropertyIds + kNumMethodIds + glue::GetStaticPropertyCount();
  PluginObject *plugin_object = static_cast<PluginObject *>(header);
  NPP npp = plugin_object->npp();
  GLUE_PROFILE_START(npp, "memalloc");
  *value = static_cast<NPIdentifier *>(
      NPN_MemAlloc(*count * sizeof(NPIdentifier)));
  GLUE_PROFILE_STOP(npp, "memalloc");
  memcpy(*value, property_ids, kNumPropertyIds * sizeof(NPIdentifier));
  memcpy(*value + kNumPropertyIds, method_ids,
         kNumMethodIds * sizeof(NPIdentifier));
  glue::StaticEnumeratePropertyHelper(
      *value + kNumPropertyIds + kNumMethodIds);
  return true;
}

static NPClass plugin_npclass = {
  NP_CLASS_STRUCT_VERSION,
  PluginAllocate,
  PluginDeallocate,
  0,
  PluginHasMethod,
  PluginInvoke,
  0,
  PluginHasProperty,
  PluginGetProperty,
  PluginSetProperty,
  0,
  PluginEnumerate,
};

PluginObject *PluginObject::Create(NPP npp) {
  GLUE_PROFILE_START(npp, "createobject");
  PluginObject *plugin_object =
      static_cast<PluginObject *>(NPN_CreateObject(npp, &plugin_npclass));
  GLUE_PROFILE_STOP(npp, "createobject");
  return plugin_object;
}

void InitializeGlue(NPP npp) {
  GLUE_PROFILE_START(npp, "getstringidentifiers");
  NPN_GetStringIdentifiers(property_names, kNumPropertyIds, property_ids);
  NPN_GetStringIdentifiers(method_names, kNumMethodIds, method_ids);
  GLUE_PROFILE_STOP(npp, "getstringidentifiers");
  glue::InitializeGlue(npp);
}

// Plugin has been resized.
void PluginObject::Resize(int width, int height) {
  // check that the window size has actually changed
  if (prev_width_ != width || prev_height_ != height) {
    prev_width_ = width;
    prev_height_ = height;

    if (renderer_ && !fullscreen_) {
      // If we are rendering offscreen, we may need to reallocate the
      // render surfaces.
      if (offscreen_rendering_enabled_) {
        AllocateOffscreenRenderSurfaces(width, height);
      }

      // Tell the renderer and client that our window has been resized.
      // If we're in fullscreen mode when this happens, we don't want to pass
      // the information through; the renderer will pick it up when we switch
      // back to plugin mode.
      renderer_->Resize(prev_width_, prev_height_);
      // This is just so that the client can send an event to the user.
      client()->SendResizeEvent(prev_width_, prev_height_, fullscreen_);
    }
  }
}

int PluginObject::width() const {
  if (renderer_) {
    return renderer_->width();
  }
  return 0;
}

int PluginObject::height() const {
  if (renderer_) {
    return renderer_->height();
  }
  return 0;
}

bool PluginObject::SetFullscreenClickRegion(int x, int y, int width, int height,
    int mode_id) {
  bool success = false;
  o3d::DisplayMode mode;
  // Make sure it's a valid ID first.
  if (GetDisplayMode(mode_id, &mode)) {
    fullscreen_region_valid_ = true;
    fullscreen_region_x_ = x;
    fullscreen_region_y_ = y;
    fullscreen_region_width_ = width;
    fullscreen_region_height_ = height;
    fullscreen_region_mode_id_ = mode_id;
    success = true;
  }
  return success;
}

// On Mac there is a different implementation in plugin_mac.mm.
#ifndef OS_MACOSX
void PluginObject::GetDisplayModes(std::vector<o3d::DisplayMode> *modes) {
  if (renderer()) {
    renderer()->GetDisplayModes(modes);
  } else {
    modes->clear();
  }
}
#endif

void PluginObject::RedirectToFile(const char *url) {
  char cmd[] = "window.location = 'file:///%s';";
  scoped_array<char> full_cmd(new char[strlen(url) + sizeof(cmd)]);
  sprintf(full_cmd.get(), cmd, url);

  NPObject *global_object;
  NPN_GetValue(npp(), NPNVWindowNPObject, &global_object);
  NPString string;
  string.UTF8Characters = full_cmd.get();
  string.UTF8Length = strlen(string.UTF8Characters);
  NPVariant result;
  bool temp = NPN_Evaluate(npp(), global_object, &string, &result);
  if (temp) {
    NPN_ReleaseVariantValue(&result);
  }
}

o3d::Cursor::CursorType PluginObject::cursor() const {
  return cursor_type_;
}

void PluginObject::set_cursor(o3d::Cursor::CursorType cursor_type) {
  cursor_type_ = cursor_type;
  PlatformSpecificSetCursor();
}

#ifdef OS_WIN
static const wchar_t* kWindowPropertyName = L"o3d";

void PluginObject::StorePluginProperty(HWND hWnd, PluginObject *obj) {
  if (obj->GetHWnd()) {  // Clear out the record from the old window first.
    ClearPluginProperty(obj->GetHWnd());
  }
  obj->SetHWnd(hWnd);
  StorePluginPropertyUnsafe(hWnd, obj);
}

void PluginObject::StorePluginPropertyUnsafe(HWND hWnd, PluginObject *obj) {
  if (hWnd) {
    SetProp(hWnd, kWindowPropertyName, static_cast<HANDLE>(obj));
    ::DragAcceptFiles(hWnd, true);
  }
}

PluginObject *PluginObject::GetPluginProperty(HWND hWnd) {
  return static_cast<PluginObject*>(GetProp(hWnd, kWindowPropertyName));
}

void PluginObject::ClearPluginProperty(HWND hWnd) {
  if (hWnd) {
    // TODO: convert to using app::win::ScopedProp.
    RemoveProp(hWnd, kWindowPropertyName);
    ::DragAcceptFiles(hWnd, false);
  }
}

static LPCTSTR O3DToWindowsCursor(o3d::Cursor::CursorType cursor_type) {
  switch (cursor_type) {
    case o3d::Cursor::DEFAULT:
      return IDC_ARROW;
    case o3d::Cursor::NONE:
      return NULL;
    case o3d::Cursor::CROSSHAIR:
      return IDC_CROSS;
    case o3d::Cursor::POINTER:
      return IDC_HAND;
    case o3d::Cursor::E_RESIZE:
      return IDC_SIZEWE;
    case o3d::Cursor::NE_RESIZE:
      return IDC_SIZENESW;
    case o3d::Cursor::NW_RESIZE:
      return IDC_SIZENWSE;
    case o3d::Cursor::N_RESIZE:
      return IDC_SIZENS;
    case o3d::Cursor::SE_RESIZE:
      return IDC_SIZENWSE;
    case o3d::Cursor::SW_RESIZE:
      return IDC_SIZENESW;
    case o3d::Cursor::S_RESIZE:
      return IDC_SIZENS;
    case o3d::Cursor::W_RESIZE:
      return IDC_SIZEWE;
    case o3d::Cursor::MOVE:
      return IDC_SIZEALL;
    case o3d::Cursor::TEXT:
      return IDC_IBEAM;
    case o3d::Cursor::WAIT:
      return IDC_WAIT;
    case o3d::Cursor::PROGRESS:
      return IDC_APPSTARTING;
    case o3d::Cursor::HELP:
      return IDC_HELP;
  }
  return IDC_ARROW;
}

void PluginObject::PlatformSpecificSetCursor() {
  if (!cursors_[cursor_type_]) {
    cursors_[cursor_type_] = ::LoadCursor(NULL,
                                          O3DToWindowsCursor(cursor_type_));
  }
  ::SetCursor(cursors_[cursor_type_]);
}

#endif  // OS_WIN

#ifdef OS_LINUX

void PluginObject::SetDisplay(Display *display) {
  if (display_ != display) {
    if (display_) {
      for (int i = 0; i < o3d::Cursor::NUM_CURSORS; ++i) {
        if (cursors_[i]) {
          XFreeCursor(display_, cursors_[i]);
          cursors_[i] = 0;
        }
      }
    }
    display_ = display;
  }
}

static unsigned int O3DToX11CursorShape(o3d::Cursor::CursorType cursor_type) {
  switch (cursor_type) {
    case o3d::Cursor::DEFAULT:
      return XC_arrow;
    case o3d::Cursor::CROSSHAIR:
      return XC_crosshair;
    case o3d::Cursor::POINTER:
      return XC_hand2;
    case o3d::Cursor::E_RESIZE:
      return XC_right_side;
    case o3d::Cursor::NE_RESIZE:
      return XC_top_right_corner;
    case o3d::Cursor::NW_RESIZE:
      return XC_top_left_corner;
    case o3d::Cursor::N_RESIZE:
      return XC_top_side;
    case o3d::Cursor::SE_RESIZE:
      return XC_bottom_right_corner;
    case o3d::Cursor::SW_RESIZE:
      return XC_bottom_left_corner;
    case o3d::Cursor::S_RESIZE:
      return XC_bottom_side;
    case o3d::Cursor::W_RESIZE:
      return XC_left_side;
    case o3d::Cursor::MOVE:
      return XC_fleur;
    case o3d::Cursor::TEXT:
      return XC_xterm;
    case o3d::Cursor::WAIT:
      return XC_watch;
    case o3d::Cursor::PROGRESS:
      NOTIMPLEMENTED();
      return XC_watch;
    case o3d::Cursor::HELP:
      NOTIMPLEMENTED();
      return XC_arrow;
  }
  NOTIMPLEMENTED();
  return XC_arrow;
}

static Cursor O3DToX11Cursor(Display *display, Window window,
                             o3d::Cursor::CursorType cursor_type) {
  switch (cursor_type) {
    case o3d::Cursor::NONE: {
      // There is no X11 primitive for hiding the cursor. The standard practice
      // is to define a custom cursor from a 1x1 invisible pixmap.
      static char zero[1] = {0};
      Pixmap zero_pixmap = XCreateBitmapFromData(display, window, zero, 1, 1);
      DLOG_ASSERT(zero_pixmap);
      if (!zero_pixmap) {
        return 0;
      }
      // This could actually be any colour, since our mask pixmap specifies that
      // no pixels are visible.
      XColor black;
      black.red = 0;
      black.green = 0;
      black.blue = 0;
      Cursor cursor = XCreatePixmapCursor(display, zero_pixmap, zero_pixmap,
                                          &black, &black, 0, 0);
      XFreePixmap(display, zero_pixmap);
      return cursor;
    }

    default:
      return XCreateFontCursor(display, O3DToX11CursorShape(cursor_type));
  }
}

void PluginObject::PlatformSpecificSetCursor() {
  if (!cursors_[cursor_type_]) {
    // According to the docs, the window here is only relevant for selecting the
    // screen, and we always create our fullscreen and embedded windows on the
    // same screen, so we can just always use the embedded window.
    cursors_[cursor_type_] = O3DToX11Cursor(display_, window_, cursor_type_);
  }
  Window window = fullscreen_ ? fullscreen_window_ : window_;
  XDefineCursor(display_, window, cursors_[cursor_type_]);
}

#endif  // OS_LINUX

void PluginObject::TickPluginObject(void* data) {
  PluginObject* plugin_object = static_cast<PluginObject*>(data);
  plugin_object->ExecuteAsyncTick();
}

void PluginObject::AsyncTick() {
  if (pending_ticks_ >= 1)
    return;

  ++pending_ticks_;

  // In Chrome NPN_PluginThreadAsyncCall doesn't seem to function properly.
  // We resort to loading a data: url with zero bytes to get a tick callback
  // asynchronously.
  // TODO(vangelis): Remove this special path when Chrome's
  // NPN_PluginThreadAsyncCall is fixed.
  if (IsChrome()) {
    class TickCallback : public StreamManager::FinishedCallback {
     public:
      explicit TickCallback(PluginObject* plugin_object)
          : plugin_object_(plugin_object) {
      }

      virtual void Run(DownloadStream*,
                       bool,
                       const std::string&,
                       const std::string&) {
        plugin_object_->ExecuteAsyncTick();
      }

     private:
      PluginObject* plugin_object_;
    };

    if (!stream_manager_->LoadURL("data:,", NULL, NULL, NULL,
                                  new TickCallback(this), NP_NORMAL)) {
      DLOG(ERROR) << "Chrome failed to access data url";
      // If the async call fails then tick synchronously.
      Tick();
    }
  } else {
    // Invoke Tick asynchronously if NPN_PluginThreadAsyncCall is supported.
    // Otherwise invoke it synchronously.
    if (IsPluginThreadAsyncCallSupported(npp_)) {
      NPN_PluginThreadAsyncCall(npp_, TickPluginObject, this);
    } else {
      Tick();
    }
  }
}

void PluginObject::ExecuteAsyncTick() {
  // Check the plugin has not been destroyed already. Chrome sometimes invokes
  // async callbacks after destruction.
  if (!client())
    return;

  // Don't allow reentrancy through asynchronous ticks. Chrome sometimes does
  // this. It is also possible for the asyncronous call to be invoked while
  // a message is being handled. This prevents that.
  Client::ScopedIncrement reentrance_count(client());
  if (reentrance_count.get() > 1) {
    --pending_ticks_;
    return;
  }

  Tick();
}

void PluginObject::Tick() {
  DCHECK_GT(pending_ticks_, 0);
  --pending_ticks_;

  client_->Tick();
  if (renderer_ && renderer_->need_to_render()) {
    client_->RenderClient(true);
  }
}

void PluginObject::EnableOffscreenRendering() {
  if (!offscreen_rendering_enabled_) {
    AllocateOffscreenRenderSurfaces(width(), height());
    offscreen_rendering_enabled_ = true;
  }
}

void PluginObject::DisableOffscreenRendering() {
  if (offscreen_rendering_enabled_) {
    DeallocateOffscreenRenderSurfaces();
    offscreen_rendering_enabled_ = false;
  }
}

bool PluginObject::IsOffscreenRenderingEnabled() const {
  return offscreen_rendering_enabled_;
}

RenderSurface::Ref PluginObject::GetOffscreenRenderSurface() const {
  return offscreen_render_surface_;
}

Bitmap::Ref PluginObject::GetOffscreenBitmap() const {
  return offscreen_readback_bitmap_;
}

bool PluginObject::AllocateOffscreenRenderSurfaces(int width, int height) {
  int pot_width =
      static_cast<int>(o3d::image::ComputePOTSize(width));
  int pot_height =
      static_cast<int>(o3d::image::ComputePOTSize(height));
  if (!renderer_ || pot_width == 0 || pot_height == 0) {
    return false;
  }
  bool must_reallocate_render_surfaces =
      (offscreen_render_surface_.IsNull() ||
       offscreen_depth_render_surface_.IsNull() ||
       offscreen_render_surface_->width() != pot_width ||
       offscreen_render_surface_->height() != pot_height);
  if (must_reallocate_render_surfaces) {
    Texture2D::Ref texture = renderer_->CreateTexture2D(
        pot_width,
        pot_height,
        Texture::ARGB8,
        1,
        true);
    if (texture.IsNull()) {
      return false;
    }
    RenderSurface::Ref surface(texture->GetRenderSurface(0));
    if (surface.IsNull()) {
      return false;
    }
    RenderDepthStencilSurface::Ref depth(renderer_->CreateDepthStencilSurface(
        pot_width,
        pot_height));
    if (depth.IsNull()) {
      return false;
    }
    offscreen_texture_ = texture;
    offscreen_render_surface_ = surface;
    offscreen_depth_render_surface_ = depth;
  }
  offscreen_render_surface_->SetClipSize(width, height);
  offscreen_depth_render_surface_->SetClipSize(width, height);
  if (offscreen_readback_bitmap_.IsNull() ||
      offscreen_readback_bitmap_->width() != width ||
      offscreen_readback_bitmap_->height() != height) {
    o3d::Bitmap::Ref bitmap = Bitmap::Ref(
        new Bitmap(service_locator()));
    bitmap->Allocate(Texture::ARGB8,
                     width, height, 1, Bitmap::IMAGE);
    offscreen_readback_bitmap_ = bitmap;
  }
  // Tell the Client about the newly allocated surfaces so that normal
  // calls to RenderClient can automatically do the right thing.
  client_->SetOffscreenRenderingSurfaces(offscreen_render_surface_,
                                         offscreen_depth_render_surface_);
  return true;
}

void PluginObject::DeallocateOffscreenRenderSurfaces() {
  if (client_) {
    client_->SetOffscreenRenderingSurfaces(RenderSurface::Ref(),
                                           RenderDepthStencilSurface::Ref());
  }
  offscreen_render_surface_.Reset();
  offscreen_depth_render_surface_.Reset();
  offscreen_readback_bitmap_.Reset();
}

}  // namespace _o3d

namespace globals {

using _o3d::PluginObject;
// This implements the definition in common.h of a function to receive
// all glue error reports.
void SetLastError(NPP npp, const char *error) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  O3D_ERROR(plugin_object->service_locator()) << error;
}

// These implement the definitions in common.h of functions to receive
// all profiling calls.
void ProfileStart(NPP npp, const std::string& key) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  if (plugin_object) {  // May not be initialized yet.
    plugin_object->client()->ProfileStart(key);
  }
}

void ProfileStop(NPP npp, const std::string& key) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  if (plugin_object) {  // May not be initialized yet.
    plugin_object->client()->ProfileStop(key);
  }
}

void ProfileReset(NPP npp) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  if (plugin_object) {  // May not be initialized yet.
    plugin_object->client()->ProfileReset();
  }
}

std::string ProfileToString(NPP npp) {
  PluginObject *plugin_object = static_cast<PluginObject *>(npp->pdata);
  if (plugin_object) {  // May not be initialized yet.
    return plugin_object->client()->ProfileToString();
  } else {
    return "";
  }
}

}  // namespace globals
}  // namespace glue
