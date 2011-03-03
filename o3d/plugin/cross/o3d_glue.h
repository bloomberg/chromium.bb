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


#ifndef O3D_PLUGIN_CROSS_O3D_GLUE_H_
#define O3D_PLUGIN_CROSS_O3D_GLUE_H_

#include <build/build_config.h>

#ifdef OS_MACOSX
#include <OpenGL/OpenGL.h>
#include <AGL/agl.h>
#include <CoreGraphics/CGContext.h>
#endif

#ifdef OS_LINUX
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#endif


#include <map>
#include <set>
#include <string>
#include <vector>
#include "base/scoped_ptr.h"
#include "base/hash_tables.h"
#include "core/cross/bitmap.h"
#include "core/cross/display_mode.h"
#include "core/cross/display_window.h"
#include "core/cross/object_base.h"
#include "core/cross/service_locator.h"
#include "core/cross/evaluation_counter.h"
#include "core/cross/class_manager.h"
#include "core/cross/client_info.h"
#include "core/cross/cursor.h"
#include "core/cross/features.h"
#include "core/cross/object_manager.h"
#include "core/cross/error.h"
#include "core/cross/profiler.h"
#include "core/cross/render_surface.h"
#include "core/cross/texture.h"
#include "plugin/cross/main_thread_task_poster.h"
#include "plugin/cross/np_v8_bridge.h"
#ifdef OS_MACOSX
#include "plugin/mac/fullscreen_window_mac.h"
#endif
#include "client_glue.h"
#include "third_party/nixysa/static_glue/npapi/common.h"

namespace o3d {
class Client;
class Renderer;
}

// Hashes the NPClass and ObjectBase types so they can be used in a hash_map.
#if defined(COMPILER_GCC)
namespace __gnu_cxx {

template<>
struct hash<NPClass*> {
  std::size_t operator()(NPClass* const& ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};

template<>
struct hash<const o3d::ObjectBase::Class*> {
  std::size_t operator()(const o3d::ObjectBase::Class* const& ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};

}  // namespace __gnu_cxx
#elif defined(COMPILER_MSVC)
namespace stdext {

template<>
inline size_t hash_value(NPClass* const& ptr) {
  return hash_value(reinterpret_cast<size_t>(ptr));
}

template<>
inline size_t hash_value(const o3d::ObjectBase::Class* const& ptr) {
  return hash_value(reinterpret_cast<size_t>(ptr));
}

}  // namespace stdext
#endif  // COMPILER

namespace glue {
class StreamManager;

namespace _o3d {
using o3d::Id;
using o3d::ObjectBase;
using o3d::Bitmap;
using o3d::Client;
using o3d::ClassManager;
using o3d::ClientInfoManager;
using o3d::EvaluationCounter;
using o3d::Features;
using o3d::EvaluationCounter;
using o3d::MainThreadTaskPoster;
using o3d::ObjectManager;
using o3d::Profiler;
using o3d::Renderer;
using o3d::RenderSurface;
using o3d::RenderDepthStencilSurface;
using o3d::ServiceLocator;
using o3d::Texture2D;
#ifdef OS_MACOSX
using o3d::FullscreenWindowMac;
#endif

class NPAPIObject: public NPObject {
  NPP npp_;
  Id id_;
  bool mapped_;
 public:
  explicit NPAPIObject(NPP npp): npp_(npp), id_(0), mapped_(false) {}
  void Initialize(const ObjectBase *object) { id_ = object->id(); }
  NPP npp() const { return npp_; }
  Id id() const { return id_; }
  bool mapped() const { return mapped_; }
  void set_mapped(bool mapped) { mapped_ = mapped; }
};
void RegisterType(NPP npp, const ObjectBase::Class *clientclass,
                  NPClass *npclass);
bool CheckObject(NPP npp, NPObject *npobject,
                 const ObjectBase::Class *clientclass);
NPAPIObject *GetNPObject(NPP npp, ObjectBase *object);
NPObject *Allocate(NPP npp, NPClass *npclass);
void Deallocate(NPObject *object);
ServiceLocator *GetServiceLocator(NPP npp);
Client *GetClient(NPP npp);
void InitializeGlue(NPP npp);

typedef glue::namespace_o3d::class_Client::NPAPIObject ClientNPObject;

class PluginObject: public NPObject {
  typedef ::base::hash_map<Id, NPAPIObject *> ClientObjectMap;
  typedef ::base::hash_map<const ObjectBase::Class *, NPClass *>
      ClientToNPClassMap;
  typedef ::base::hash_map<NPClass *, const ObjectBase::Class *>
      NPToClientClassMap;

  NPP npp_;
  ServiceLocator service_locator_;
  EvaluationCounter evaluation_counter_;
  ClassManager class_manager_;
  ClientInfoManager client_info_manager_;
  ObjectManager object_manager_;
  Profiler profiler_;
  MainThreadTaskPoster main_thread_task_poster_;
  bool fullscreen_;  // Are we rendered fullscreen or in the plugin region?
  Renderer *renderer_;
  Client *client_;
  Features* features_;
  ClientObjectMap object_map_;
  ClientToNPClassMap client_to_np_class_map_;
  NPToClientClassMap np_to_client_class_map_;
  NPObject *globals_npobject_;
  ClientNPObject *client_npobject_;
  std::string user_agent_;
  Renderer::InitStatus renderer_init_status_;
  int pending_ticks_;

  // The current cursor type.
  o3d::Cursor::CursorType cursor_type_;

  // Last known dimensions of plugin
  int prev_width_;
  int prev_height_;

  o3d::NPV8Bridge np_v8_bridge_;
  scoped_ptr<StreamManager> stream_manager_;

 public:
#ifdef OS_WIN
  void SetHWnd(HWND hWnd) {
    hWnd_ = hWnd;
  }
  HWND GetHWnd() {
    return hWnd_;
  }
  void SetPluginHWnd(HWND hWnd) {
    plugin_hWnd_ = hWnd;
  }
  HWND GetPluginHWnd() {
    return plugin_hWnd_;
  }
  void SetContentHWnd(HWND hWnd) {
    content_hWnd_ = hWnd;
  }
  HWND GetContentHWnd() {
    return content_hWnd_;
  }
  bool RecordPaint() {
    bool painted = painted_once_;
    painted_once_ = true;
    return painted;
  }
  bool got_dblclick() const { return got_dblclick_; }
  void set_got_dblclick(bool got_dblclick) { got_dblclick_ = got_dblclick; }
#elif defined(OS_LINUX)
  void SetGtkEventSource(GtkWidget *widget);
  gboolean OnGtkConfigure(GtkWidget *widget,
                          GdkEventConfigure *configure_event);
  gboolean OnGtkDelete(GtkWidget *widget,
                       GdkEvent *configure);
  void SetDisplay(Display *display);
#elif defined(OS_MACOSX)
  FullscreenWindowMac* GetFullscreenMacWindow() {
    return mac_fullscreen_window_;
  }

  void SetFullscreenMacWindow(FullscreenWindowMac* window) {
    mac_fullscreen_window_ = window;
  }

  WindowRef GetMacWindow() {
    return mac_window_;
  }

  bool ScrollIsInProgress() { return scroll_is_in_progress_; }
  void SetScrollIsInProgress(bool state) { scroll_is_in_progress_ = state; }
  bool scroll_is_in_progress_;

  bool RendererIsSoftware() {return renderer_is_software_;}
  bool SetRendererIsSoftware(bool state);
  bool renderer_is_software_;

  AGLContext GetMacAGLContext() {
    return mac_agl_context_;
  }

  CGLContextObj GetMacCGLContext() {
    return mac_cgl_context_;
  }

  void SetMacCGLContext(CGLContextObj obj);

  CGLContextObj GetFullscreenShareContext();
  CGLPixelFormatObj GetFullscreenCGLPixelFormatObj();
  void* GetFullscreenNSOpenGLContext();
  void CleanupFullscreenOpenGLContext();

  NPDrawingModel drawing_model_;
  NPEventModel event_model_;
  WindowRef mac_window_;  // may be NULL in the Chrome case
  // these vars needed for the Safari tab switch detection hack
  CFDateRef last_mac_event_time_;
  void * mac_cocoa_window_;
  void* mac_window_selected_tab_;
  bool mac_surface_hidden_;
  // end of Safari tab detection vars
  GLint last_buffer_rect_[4];
  Point last_plugin_loc_;
  // each of these three can be NULL depending on drawing_model
  CGContextRef mac_cg_context_ref_;
  AGLContext mac_agl_context_;
  CGLContextObj mac_cgl_context_;
  void *gl_layer_;
  // If in Chrome, we actually do all of our rendering offscreen, and
  // bootstrap off a 1x1 pbuffer
  CGLPBufferObj mac_cgl_pbuffer_;

  // Fullscreen related stuff.
  // NULL if not in fullscreen mode.
  // Must not be a scoped_ptr due to reentrancy during termination.
  FullscreenWindowMac* mac_fullscreen_window_;
  // When rendering using CGL, we need to use an NSOpenGLContext to
  // implement full-screen support. In order to share textures and
  // other resources between the core CGL context and the full-screen
  // one, we need to allocate the NSOpenGLContext first, because with
  // pre-10.6 APIs it isn't possible to make an NSOpenGLContext share
  // resources with a preexisting CGLContextObj.
  void* mac_fullscreen_nsopenglcontext_;
  // On 10.5 (Core Graphics drawing model) it appears that we need to
  // forcibly reuse the CGLContextObj from the NSOpenGLPixelFormat
  // that we use to create the NSOpenGLContext for full-screen, or
  // the share context is reported invalid.
  void* mac_fullscreen_nsopenglpixelformat_;
  // Indication when we transition to full-screen mode of whether we
  // were using off-screen rendering (Core Graphics drawing model, in
  // particular).
  bool was_offscreen_;

#endif  //  OS_MACOSX
#ifdef OS_LINUX
  Window window_;
  Window fullscreen_window_;

  // Xt mode
  Widget xt_widget_;
  XtAppContext xt_app_context_;
  XtIntervalId xt_interval_;
  Time last_click_time_;

  // XEmbed mode
  Window drawable_;
  GdkDisplay *gdk_display_;
  GtkWidget *gtk_container_;
  GtkWidget *gtk_fullscreen_container_;
  GtkWidget *gtk_event_source_;
  gulong event_handler_id_;
  bool got_double_click_[3];
  guint timeout_id_;
  bool fullscreen_pending_;

  bool draw_;
  bool in_plugin_;
#endif
  explicit PluginObject(NPP npp);
  ~PluginObject();
  void Init(int argc, char* argn[], char* argv[]);
  void TearDown();
  ServiceLocator* service_locator() { return &service_locator_; }
  Client *client() { return client_; }
  Renderer *renderer() { return renderer_; }
  NPP npp() { return npp_; }
  void RegisterType(const ObjectBase::Class *clientclass, NPClass *npclass);
  bool CheckObject(NPObject *npobject,
                   const ObjectBase::Class *clientclass) const;
  NPAPIObject *GetNPObject(ObjectBase *object);
  void UnmapObject(NPAPIObject *npobject);
  void UnmapAll();
  NPClass *GetNPClass(const ObjectBase::Class *clientclass);

  NPObject *globals_npobject() { return globals_npobject_; }
  ClientNPObject *client_npobject() { return client_npobject_; }

  static const char* kOnRenderEventName;

  static PluginObject *Create(NPP npp);

  StreamManager *stream_manager() const { return stream_manager_.get(); }

  static void LogAssertHandlerFunction(const std::string& str);

  Renderer::InitStatus renderer_init_status() const {
    return renderer_init_status_;
  }

  // Plugin has been resized.
  void Resize(int width, int height);

  int width() const;
  int height() const;

  // Fullscreen stuff
  bool fullscreen() const {
    return fullscreen_;
  }

  // Fetch one mode by externally visible id, returning true on success.
  bool GetDisplayMode(int id, o3d::DisplayMode *mode);

  // Get a vector of the available fullscreen display modes.
  // Clears *modes on error.
  void GetDisplayModes(std::vector<o3d::DisplayMode> *modes);

  // This isn't exposed directly to JavaScript for security reasons; it gets
  // called by the platform-specific event-handling code if the region set by
  // SetFullscreenClickRegion is clicked.  It requests the mode previously set
  // by SetFullscreenClickRegion(), and fails if there wasn't one.
#ifdef OS_LINUX
  bool RequestFullscreenDisplay(guint32 timestamp);
#else
  bool RequestFullscreenDisplay();
#endif

  // Make a region of the plugin area that will invoke fullscreen mode if
  // clicked.  The app developer is responsible for communicating this to the
  // user, as this region has no visible marker.  The developer is also
  // responsible for updating this region if the plugin gets resized, as we
  // don't know whether or how to scale it.
  // Fails if the mode_id supplied isn't valid.  Returns true on success.
  bool SetFullscreenClickRegion(int x, int y, int width, int height,
      int mode_id);
  void ClearFullscreenClickRegion() {
    fullscreen_region_valid_ = false;
  }

  // Tests a mouse click location for a hit on the fullscreen click region.
  bool HitFullscreenClickRegion(int x, int y) {
    if (!fullscreen_region_valid_ || fullscreen_) {
      return false;
    }
    return (x >= fullscreen_region_x_) &&
        (x - fullscreen_region_x_ < fullscreen_region_width_) &&
        (y >= fullscreen_region_y_) &&
        (y - fullscreen_region_y_ < fullscreen_region_height_);
  }

  // Deactivate fullscreen mode.
  void CancelFullscreenDisplay();

  // Redirect by setting window.location to file:///<url>.
  void RedirectToFile(const char *url);

  // Get the cursor's shape.
  o3d::Cursor::CursorType cursor() const;

  // Set the cursor's shape.
  void set_cursor(o3d::Cursor::CursorType cursor_type);

  // Sets the cursor to whatever the current cursor is.
  void PlatformSpecificSetCursor();

  // Asynchronously (if possible, synchronously otherwise) invoke Tick. No
  // operation if an asynchronous tick is already pending.
  void AsyncTick();

  // Tick the client.
  void Tick();

  const std::string& user_agent() const { return user_agent_; }
  bool IsFirefox() const {
    return user_agent_.find("Firefox") != user_agent_.npos;
  }
  bool IsChrome() const {
    return user_agent_.find("Chrome") != user_agent_.npos;
  }
  bool IsMSIE() const {
    return user_agent_.find("MSIE") != user_agent_.npos;
  }

  o3d::NPV8Bridge* np_v8_bridge() { return &np_v8_bridge_; }

  // Creates the renderer.
  void CreateRenderer(const o3d::DisplayWindow& display_window);

  // Deletes the renderer.
  void DeleteRenderer();

#ifdef OS_MACOSX
  // Methods needed for the Safari tab-switching hack.
  void MacEventReceived(bool updateTab);

  CFTimeInterval TimeSinceLastMacEvent();

  bool DetectTabHiding();
#endif

#ifdef OS_WIN
  // Stashes a pointer to the plugin on the HWND, and keeps a corresponding
  // pointer to the HWND on the plugin.  If the plugin's already registered with
  // an HWND, this will clear out the plugin pointer on the old HWND before
  // setting up the new one.
  static void StorePluginProperty(HWND hWnd, PluginObject *obj);
  // Similar to StorePluginProperty, but doesn't clear out any old pointers.
  static void StorePluginPropertyUnsafe(HWND hWnd, PluginObject *obj);
  // Looks up the plugin stored on this HWND.
  static PluginObject *GetPluginProperty(HWND hWnd);
  // Clears out the plugin stored on this HWND.
  static void ClearPluginProperty(HWND hWnd);
  // One bit of state that helps us turn the sequence of events that windows
  // produces on a double click into what JavaScript is expecting.
  bool got_dblclick_;
#endif  // OS_WIN
#ifdef OS_LINUX
  bool in_plugin() const { return in_plugin_; }
  void set_in_plugin(bool value) { in_plugin_ = value; }
  Time last_click_time() const { return last_click_time_; }
  void set_last_click_time(Time value) { last_click_time_ = value; }
#endif

#if defined(CB_SERVICE_REMOTE)
  void SetGPUPluginObject(NPObject* gpu_plugin_object);
  NPObject* GetGPUPluginObject() {
    return gpu_plugin_object_;
  }
#endif

  // Support for rendering the plugin's content into render surfaces.
  // This is currently different than offscreen rendering support in
  // the renderer. It is a bit of a mess because only the PluginObject
  // and Renderer have the necessary information about the viewport
  // size, and only the Client has enough information to properly
  // initiate a render.
  void EnableOffscreenRendering();
  void DisableOffscreenRendering();
  bool IsOffscreenRenderingEnabled() const;
  RenderSurface::Ref GetOffscreenRenderSurface() const;
  RenderDepthStencilSurface::Ref GetOffscreenDepthRenderSurface() const;
  Bitmap::Ref GetOffscreenBitmap() const;

 private:
  // Called through the browser's NPN_PluginThreadAsyncCall
  static void TickPluginObject(void* data);

  // Completes the work of an AsyncTick().
  void ExecuteAsyncTick();

  bool fullscreen_region_valid_;
  int fullscreen_region_x_;
  int fullscreen_region_y_;
  int fullscreen_region_width_;
  int fullscreen_region_height_;
  int fullscreen_region_mode_id_;
#ifdef OS_WIN
  HWND hWnd_;          // The window we are currently drawing to (use this).
  HWND plugin_hWnd_;   // The window we were given inside the browser.
  HWND content_hWnd_;  // The window containing the D3D or OpenGL content.
  HCURSOR cursors_[o3d::Cursor::NUM_CURSORS];  // loaded windows cursors.
  HCURSOR hCursor_;
  bool painted_once_;
#elif defined(OS_LINUX)
  Display *display_;
  Cursor cursors_[o3d::Cursor::NUM_CURSORS];  // loaded windows cursors.
#endif  // OS_WIN

#if defined(CB_SERVICE_REMOTE)
  NPObject* gpu_plugin_object_;
#endif

  // Support for offscreen rendering and thereby windowless plugins.
  // Currently used only for the CoreGraphics drawing model on Mac OS
  // X when there is no WindowRef. It would be nicer to put this in
  // the Renderer and/or make its off_screen mode work everywhere, but
  // this is only a legacy solution for older browsers.
  bool offscreen_rendering_enabled_;
  Texture2D::Ref offscreen_texture_;
  RenderSurface::Ref offscreen_render_surface_;
  RenderDepthStencilSurface::Ref offscreen_depth_render_surface_;
  Bitmap::Ref offscreen_readback_bitmap_;

  bool AllocateOffscreenRenderSurfaces(int width, int height);
  void DeallocateOffscreenRenderSurfaces();
};

}  // namespace o3d
}  // namespace glue


#endif  // O3D_PLUGIN_CROSS_O3D_GLUE_H_
