// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_DELEGATE_IMPL_H_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_DELEGATE_IMPL_H_

#include <string>
#include <list>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/plugins/npapi/webplugin_delegate.h"
#include "webkit/glue/webcursor.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"

typedef struct _GdkDrawable GdkPixmap;
#endif

class FilePath;

namespace WebKit {
class WebMouseEvent;
}

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class CALayer;
@class CARenderer;
#else
class CALayer;
class CARenderer;
#endif
#endif

namespace webkit {
namespace npapi {

class PluginInstance;

#if defined(OS_MACOSX)
class WebPluginAcceleratedSurface;
class ExternalDragTracker;
#ifndef NP_NO_QUICKDRAW
class QuickDrawDrawingManager;
#endif  // NP_NO_QUICKDRAW
#endif  // OS_MACOSX

// An implementation of WebPluginDelegate that runs in the plugin process,
// proxied from the renderer by WebPluginDelegateProxy.
class WebPluginDelegateImpl : public WebPluginDelegate {
 public:
  enum PluginQuirks {
    PLUGIN_QUIRK_SETWINDOW_TWICE = 1,  // Win32
    PLUGIN_QUIRK_THROTTLE_WM_USER_PLUS_ONE = 2,  // Win32
    PLUGIN_QUIRK_DONT_CALL_WND_PROC_RECURSIVELY = 4,  // Win32
    PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY = 8,  // Win32
    PLUGIN_QUIRK_DONT_ALLOW_MULTIPLE_INSTANCES = 16,  // Win32
    PLUGIN_QUIRK_DIE_AFTER_UNLOAD = 32,  // Win32
    PLUGIN_QUIRK_PATCH_SETCURSOR = 64,  // Win32
    PLUGIN_QUIRK_BLOCK_NONSTANDARD_GETURL_REQUESTS = 128,  // Win32
    PLUGIN_QUIRK_WINDOWLESS_OFFSET_WINDOW_TO_DRAW = 256,  // Linux
    PLUGIN_QUIRK_WINDOWLESS_INVALIDATE_AFTER_SET_WINDOW = 512,  // Linux
    PLUGIN_QUIRK_NO_WINDOWLESS = 1024,  // Windows
    PLUGIN_QUIRK_PATCH_REGENUMKEYEXW = 2048,  // Windows
    PLUGIN_QUIRK_ALWAYS_NOTIFY_SUCCESS = 4096,  // Windows
    PLUGIN_QUIRK_ALLOW_FASTER_QUICKDRAW_PATH = 8192,  // Mac
    PLUGIN_QUIRK_HANDLE_MOUSE_CAPTURE = 16384,  // Windows
    PLUGIN_QUIRK_WINDOWLESS_NO_RIGHT_CLICK = 32768,  // Linux
    PLUGIN_QUIRK_IGNORE_FIRST_SETWINDOW_CALL = 65536,  // Windows.
  };

  static WebPluginDelegateImpl* Create(const FilePath& filename,
                                       const std::string& mime_type,
                                       gfx::PluginWindowHandle containing_view);

  static bool IsPluginDelegateWindow(gfx::NativeWindow window);
  static bool GetPluginNameFromWindow(gfx::NativeWindow window,
                                      string16* plugin_name);

  // Returns true if the window handle passed in is that of the dummy
  // activation window for windowless plugins.
  static bool IsDummyActivationWindow(gfx::NativeWindow window);

  // WebPluginDelegate implementation
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          WebPlugin* plugin,
                          bool load_manually);
  virtual void PluginDestroyed();
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  virtual void Paint(WebKit::WebCanvas* canvas, const gfx::Rect& rect);
  virtual void Print(gfx::NativeDrawingContext context);
  virtual void SetFocus(bool focused);
  virtual bool HandleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo* cursor_info);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(
      const GURL& url, NPReason reason, int notify_id);
  virtual int GetProcessId();
  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success,
                                    int notify_id);
  virtual void DidReceiveManualResponse(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified);
  virtual void DidReceiveManualData(const char* buffer, int length);
  virtual void DidFinishManualLoading();
  virtual void DidManualLoadFail();
  virtual void InstallMissingPlugin();
  virtual WebPluginResourceClient* CreateResourceClient(
      unsigned long resource_id, const GURL& url, int notify_id);
  virtual WebPluginResourceClient* CreateSeekableResourceClient(
      unsigned long resource_id, int range_request_id);
  // End of WebPluginDelegate implementation.

  bool IsWindowless() const { return windowless_ ; }
  gfx::Rect GetRect() const { return window_rect_; }
  gfx::Rect GetClipRect() const { return clip_rect_; }

  // Returns the path for the library implementing this plugin.
  FilePath GetPluginPath();

  // Returns a combination of PluginQuirks.
  int GetQuirks() const { return quirks_; }

  // Informs the plugin that the view it is in has gained or lost focus.
  void SetContentAreaHasFocus(bool has_focus);

#if defined(OS_MACOSX)
  // Informs the plugin that the geometry has changed, as with UpdateGeometry,
  // but also includes the new buffer context for that new geometry.
  void UpdateGeometryAndContext(const gfx::Rect& window_rect,
                                const gfx::Rect& clip_rect,
                                gfx::NativeDrawingContext context);
  // Informs the delegate that the plugin called NPN_Invalidate*. Used as a
  // trigger for Core Animation drawing.
  void PluginDidInvalidate();
  // Returns the delegate currently processing events.
  static WebPluginDelegateImpl* GetActiveDelegate();
  // Informs the plugin that the window it is in has gained or lost focus.
  void SetWindowHasFocus(bool has_focus);
  // Returns whether or not the window the plugin is in has focus.
  bool GetWindowHasFocus() const { return containing_window_has_focus_; }
  // Informs the plugin that its tab or window has been hidden or shown.
  void SetContainerVisibility(bool is_visible);
  // Informs the plugin that its containing window's frame has changed.
  // Frames are in screen coordinates.
  void WindowFrameChanged(const gfx::Rect& window_frame,
                          const gfx::Rect& view_frame);
  // Informs the plugin that IME composition has been confirmed.
  void ImeCompositionConfirmed(const string16& text);
  // Informs the delegate that the plugin set a Carbon ThemeCursor.
  void SetThemeCursor(ThemeCursor cursor);
  // Informs the delegate that the plugin set a Carbon Cursor.
  void SetCursor(const Cursor* cursor);
  // Informs the delegate that the plugin set a Cocoa NSCursor.
  void SetNSCursor(NSCursor* cursor);

#ifndef NP_NO_CARBON
  // Indicates that it's time to send the plugin a null event.
  void FireIdleEvent();
#endif
#endif  // OS_MACOSX

  gfx::PluginWindowHandle windowed_handle() const {
    return windowed_handle_;
  }

#if defined(OS_MACOSX)
  // Allow setting a "fake" window handle to associate this plug-in with
  // an IOSurface in the browser. Used for accelerated drawing surfaces.
  void set_windowed_handle(gfx::PluginWindowHandle handle);
#endif

#if defined(USE_X11)
  void SetWindowlessShmPixmap(XID shm_pixmap) {
    windowless_shm_pixmap_ = shm_pixmap;
  }
#endif

 private:
  friend class DeleteTask<WebPluginDelegateImpl>;
  friend class WebPluginDelegate;

  WebPluginDelegateImpl(gfx::PluginWindowHandle containing_view,
                        PluginInstance *instance);
  ~WebPluginDelegateImpl();

  // Called by Initialize() for platform-specific initialization.
  // If this returns false, the plugin shouldn't be started--see Initialize().
  bool PlatformInitialize();

  // Called by DestroyInstance(), used for platform-specific destruction.
  void PlatformDestroyInstance();

  //--------------------------
  // used for windowed plugins
  void WindowedUpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  // Create the native window.
  // Returns true if the window is created (or already exists).
  // Returns false if unable to create the window.
  bool WindowedCreatePlugin();

  // Destroy the native window.
  void WindowedDestroyWindow();

  // Reposition the native window to be in sync with the given geometry.
  // Returns true if the native window has moved or been clipped differently.
  bool WindowedReposition(const gfx::Rect& window_rect,
                          const gfx::Rect& clip_rect);

  // Tells the plugin about the current state of the window.
  // See NPAPI NPP_SetWindow for more information.
  void WindowedSetWindow();

#if defined(OS_WIN)
  // Registers the window class for our window
  ATOM RegisterNativeWindowClass();

  // Our WndProc functions.
  static LRESULT CALLBACK DummyWindowProc(
      HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK NativeWndProc(
      HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK FlashWindowlessWndProc(
      HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Used for throttling Flash messages.
  static void ClearThrottleQueueForWindow(HWND window);
  static void OnThrottleMessage();
  static void ThrottleMessage(WNDPROC proc, HWND hwnd, UINT message,
      WPARAM wParam, LPARAM lParam);
#endif

  //----------------------------
  // used for windowless plugins
  void WindowlessUpdateGeometry(const gfx::Rect& window_rect,
                                const gfx::Rect& clip_rect);
  void WindowlessPaint(gfx::NativeDrawingContext hdc, const gfx::Rect& rect);

  // Tells the plugin about the current state of the window.
  // See NPAPI NPP_SetWindow for more information.
  void WindowlessSetWindow();

  // Informs the plugin that it has gained or lost keyboard focus (on the Mac,
  // this just means window first responder status).
  void SetPluginHasFocus(bool focused);

  // Handles the platform specific details of setting plugin focus. Returns
  // false if the platform cancelled the focus tranfer.
  bool PlatformSetPluginHasFocus(bool focused);

  //-----------------------------------------
  // used for windowed and windowless plugins

  // Does platform-specific event handling. Arguments and return are identical
  // to HandleInputEvent.
  bool PlatformHandleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo* cursor_info);

  PluginInstance* instance() { return instance_.get(); }

  // Closes down and destroys our plugin instance.
  void DestroyInstance();


  // used for windowed plugins
  // Note: on Mac OS X, the only time the windowed handle is non-zero
  // is the case of accelerated rendering, which uses a fake window handle to
  // identify itself back to the browser. It still performs all of its
  // work offscreen.
  gfx::PluginWindowHandle windowed_handle_;
  gfx::Rect windowed_last_pos_;

  bool windowed_did_set_window_;

  // used by windowed and windowless plugins
  bool windowless_;

  WebPlugin* plugin_;
  scoped_refptr<PluginInstance> instance_;

#if defined(OS_WIN)
  // Original wndproc before we subclassed.
  WNDPROC plugin_wnd_proc_;

  // Used to throttle WM_USER+1 messages in Flash.
  uint32 last_message_;
  bool is_calling_wndproc;

  // The current keyboard layout of this process and the main thread ID of the
  // browser process. These variables are used for synchronizing the keyboard
  // layout of this process with the one of the browser process.
  HKL keyboard_layout_;
  int parent_thread_id_;
#endif  // defined(OS_WIN)

#if defined(USE_X11)
  // The SHM pixmap for a windowless plugin.
  XID windowless_shm_pixmap_;

  // The pixmap we're drawing into, for a windowless plugin.
  GdkPixmap* pixmap_;
  double first_event_time_;

  // On Linux some plugins assume that the GtkSocket container is in the same
  // process. So we create a GtkPlug to plug into the browser's container, and
  // a GtkSocket to hold the plugin. We then send the GtkPlug to the browser
  // process.
  GtkWidget* plug_;
  GtkWidget* socket_;

  // Ensure pixmap_ exists and is at least width by height pixels.
  void EnsurePixmapAtLeastSize(int width, int height);
#endif

  gfx::PluginWindowHandle parent_;
  NPWindow window_;
  gfx::Rect window_rect_;
  gfx::Rect clip_rect_;
  int quirks_;

#if defined(OS_WIN)
  // Windowless plugins don't have keyboard focus causing issues with the
  // plugin not receiving keyboard events if the plugin enters a modal
  // loop like TrackPopupMenuEx or MessageBox, etc.
  // This is a basic issue with windows activation and focus arising due to
  // the fact that these windows are created by different threads. Activation
  // and focus are thread specific states, and if the browser has focus,
  // the plugin may not have focus.
  // To fix a majority of these activation issues we create a dummy visible
  // child window to which we set focus whenever the windowless plugin
  // receives a WM_LBUTTONDOWN/WM_RBUTTONDOWN message via NPP_HandleEvent.

  HWND dummy_window_for_activation_;
  bool CreateDummyWindowForActivation();

  // Returns true if the event passed in needs to be tracked for a potential
  // modal loop.
  static bool ShouldTrackEventForModalLoops(NPEvent* event);

  // The message filter hook procedure, which tracks modal loops entered by
  // a plugin in the course of a NPP_HandleEvent call.
  static LRESULT CALLBACK HandleEventMessageFilterHook(int code, WPARAM wParam,
                                                       LPARAM lParam);

  // TrackPopupMenu interceptor. Parameters are the same as the Win32 function
  // TrackPopupMenu.
  static BOOL WINAPI TrackPopupMenuPatch(HMENU menu, unsigned int flags, int x,
                                         int y, int reserved, HWND window,
                                         const RECT* rect);

  // SetCursor interceptor for windowless plugins.
  static HCURSOR WINAPI SetCursorPatch(HCURSOR cursor);

  // RegEnumKeyExW interceptor.
  static LONG WINAPI RegEnumKeyExWPatch(
      HKEY key, DWORD index, LPWSTR name, LPDWORD name_size, LPDWORD reserved,
      LPWSTR class_name, LPDWORD class_size, PFILETIME last_write_time);

  // The mouse hook proc which handles mouse capture in windowed plugins.
  static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam,
                                        LPARAM lParam);

  // Calls SetCapture/ReleaseCapture based on the message type.
  static void HandleCaptureForMessage(HWND window, UINT message);

#elif defined(OS_MACOSX)
  // Sets window_rect_ to |rect|
  void SetPluginRect(const gfx::Rect& rect);
  // Sets content_area_origin to |origin|
  void SetContentAreaOrigin(const gfx::Point& origin);
  // Updates everything that depends on the plugin's absolute screen location.
  void PluginScreenLocationChanged();
  // Updates anything that depends on plugin visibility.
  void PluginVisibilityChanged();

  // Enables/disables IME.
  void SetImeEnabled(bool enabled);

  // Informs the browser about the updated accelerated drawing surface.
  void UpdateAcceleratedSurface();

  // Uses a CARenderer to draw the plug-in's layer in our OpenGL surface.
  void DrawLayerInSurface();

#ifndef NP_NO_CARBON
  // Moves our dummy window to match the current screen location of the plugin.
  void UpdateDummyWindowBounds(const gfx::Point& plugin_origin);

#ifndef NP_NO_QUICKDRAW
  // Sets the mode used for QuickDraw plugin drawing. If enabled is true the
  // plugin draws into a GWorld that's not connected to a window (the faster
  // path), otherwise the plugin draws into our invisible dummy window (which is
  // slower, since the call we use to scrape the window contents is much more
  // expensive than copying between GWorlds).
  void SetQuickDrawFastPathEnabled(bool enabled);
#endif

  // Adjusts the idle event rate for a Carbon plugin based on its current
  // visibility.
  void UpdateIdleEventRate();
#endif  // !NP_NO_CARBON

  CGContextRef buffer_context_;  // Weak ref.

#ifndef NP_NO_CARBON
  NP_CGContext np_cg_context_;
#endif
#ifndef NP_NO_QUICKDRAW
  NP_Port qd_port_;
  scoped_ptr<QuickDrawDrawingManager> qd_manager_;
  base::TimeTicks fast_path_enable_tick_;
#endif

  CALayer* layer_;  // Used for CA drawing mode. Weak, retained by plug-in.
  WebPluginAcceleratedSurface* surface_;  // Weak ref.
  CARenderer* renderer_;  // Renders layer_ to surface_.
  scoped_ptr<base::RepeatingTimer<WebPluginDelegateImpl> > redraw_timer_;

  // The upper-left corner of the web content area in screen coordinates,
  // relative to an upper-left (0,0).
  gfx::Point content_area_origin_;

  bool containing_window_has_focus_;
  bool initial_window_focus_;
  bool container_is_visible_;
  bool have_called_set_window_;

  gfx::Rect cached_clip_rect_;

  bool ime_enabled_;

  scoped_ptr<ExternalDragTracker> external_drag_tracker_;
#endif  // OS_MACOSX

  // Called by the message filter hook when the plugin enters a modal loop.
  void OnModalLoopEntered();

  // Returns true if the message passed in corresponds to a user gesture.
  static bool IsUserGesture(const WebKit::WebInputEvent& event);

  // The url with which the plugin was instantiated.
  std::string plugin_url_;

#if defined(OS_WIN)
  // Indicates the end of a user gesture period.
  void OnUserGestureEnd();

  // Handle to the message filter hook
  HHOOK handle_event_message_filter_hook_;

  // Event which is set when the plugin enters a modal loop in the course
  // of a NPP_HandleEvent call.
  HANDLE handle_event_pump_messages_event_;

  // This flag indicates whether we started tracking a user gesture message.
  bool user_gesture_message_posted_;

  // Runnable Method Factory used to invoke the OnUserGestureEnd method
  // asynchronously.
  ScopedRunnableMethodFactory<WebPluginDelegateImpl> user_gesture_msg_factory_;

  // Handle to the mouse hook installed for certain windowed plugins like
  // flash.
  HHOOK mouse_hook_;
#endif

  // Holds the depth of the HandleEvent callstack.
  int handle_event_depth_;

  // Holds the current cursor set by the windowless plugin.
  WebCursor current_windowless_cursor_;

  // Set to true initially and indicates if this is the first npp_setwindow
  // call received by the plugin.
  bool first_set_window_call_;

  // True if the plugin thinks it has keyboard focus
  bool plugin_has_focus_;
  // True if the plugin element has focus within the web content, regardless of
  // whether its containing view currently has focus.
  bool has_webkit_focus_;
  // True if the containing view currently has focus.
  // Initially set to true so that plugin focus still works in environments
  // where SetContentAreaHasFocus is never called. See
  // https://bugs.webkit.org/show_bug.cgi?id=46013 for details.
  bool containing_view_has_focus_;

  // True if NPP_New did not return an error.
  bool creation_succeeded_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegateImpl);
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGIN_DELEGATE_IMPL_H_
