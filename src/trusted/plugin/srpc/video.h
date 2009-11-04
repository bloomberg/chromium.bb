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

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_VIDEO_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_VIDEO_H_

#ifdef NACL_STANDALONE

#if NACL_OSX
#include <Carbon/Carbon.h>
#endif  // NACL_OSX
#if NACL_WINDOWS
#include <windows.h>
#include <windowsx.h>
#endif  // NACL_WINDOWS
#if NACL_LINUX && defined(MOZ_X11) && NACL_BUILD_ARCH != arm
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#endif  // NACL_LINUX && defined(MOZ_X11) && NACL_BUILD_ARCH != arm

#else  //  NACL_STANDALONE
// Don't include video support in Chrome build

#endif  //  NACL_STANDALONE

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/service_runtime/include/sys/audio_video.h"
#include "native_client/src/untrusted/av/nacl_av_priv.h"
#include "native_client/src/shared/imc/nacl_htp.h"

#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/npinstance.h"
#include "native_client/src/trusted/plugin/srpc/srpc.h"

typedef NPWindow PluginWindow;

// TODO(nfullagar): prune headers.
#include "native_client/src/trusted/plugin/srpc/multimedia_socket.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/shared_memory.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"


namespace nacl {

enum VideoUpdateMode {
  kVideoUpdatePluginPaint = 0,   // update via browser plugin paint
  kVideoUpdateAsynchronous = 1   // update asynchronously (faster)
};

enum {
  kClear = 0,
  kSet = 1
};

struct VideoCallbackData {
  NaClHandle handle;
  PortablePluginInterface *portable_plugin;
  int  refcount;
  nacl_srpc::MultimediaSocket *msp;

  VideoCallbackData(NaClHandle h,
                    PortablePluginInterface *p,
                    int r,
                    nacl_srpc::MultimediaSocket *sockp):
      handle(h), portable_plugin(p), refcount(r), msp(sockp) {}
};

class VideoMap {
 public:

  void Disable() { video_enabled_ = false; }
  void Enable() { video_enabled_ = true; }
  int EventQueuePut(union NaClMultimediaEvent *event);
  int EventQueueIsFull();
  static void ForceDeleteCallbackData(VideoCallbackData *vcd);
  int GetButton();
  int GetKeyMod();
  void GetMotion(uint16_t *x, uint16_t *y);
  void GetRelativeMotion(uint16_t x, uint16_t y,
      int16_t *rel_x, int16_t *rel_y);
  int32_t GetVideoUpdateMode() { return video_update_mode_; }
  int16_t HandleEvent(void* event);
  int InitializeSharedMemory(PluginWindow* window);
  VideoCallbackData* InitCallbackData(NaClHandle h,
                                      PortablePluginInterface *p,
                                      nacl_srpc::MultimediaSocket *msp);
  void Invalidate();
  bool IsEnabled() { return video_enabled_; }
  int Paint();
  void Redraw();
  void RedrawAsync(void *platform_specific);
  static VideoCallbackData* ReleaseCallbackData(VideoCallbackData *vcd);
  void RequestRedraw();
  void SetButton(int button, int state);
  void SetMotion(uint16_t x, uint16_t y, int last_valid);
  void SetKeyMod(int nsym, int state);
  bool SetWindow(PluginWindow* window);
  void set_platform_specific(void *sp) { platform_specific_ = sp; }
  void* platform_specific() { return platform_specific_; }
  HtpHandle video_handle() { return video_handle_; }
  nacl_srpc::ScriptableHandle<nacl_srpc::SharedMemory>* video_shared_memory()
      { return video_shared_memory_; }
  nacl_srpc::ScriptableHandle<nacl_srpc::SharedMemory>*
      VideoSharedMemorySetup();

  explicit VideoMap(PortablePluginInterface *plugin_interface);
  ~VideoMap();

#ifdef NACL_STANDALONE
#if NACL_WINDOWS
  WNDPROC                  original_window_procedure_;
  static LRESULT CALLBACK  WindowProcedure(HWND hwnd,
                                           UINT msg,
                                           WPARAM wparam,
                                           LPARAM lparam);
#endif  // NACL_WINDOWS
#if NACL_LINUX && defined(MOZ_X11) && NACL_BUILD_ARCH != arm
  static void XEventHandler(Widget widget,
                            VideoMap* video,
                            XEvent* xevent,
                            Boolean* b);
#endif  // NACL_LINUX && defined(MOZ_X11) && NACL_BUILD_ARCH != arm
#endif  // NACL_STANDALONE

 private:
  volatile int             event_state_button_;
  volatile int             event_state_key_mod_;
  volatile uint16_t        event_state_motion_last_x_;
  volatile uint16_t        event_state_motion_last_y_;
  volatile int             event_state_motion_last_valid_;
  void*                    platform_specific_;
  volatile bool            request_redraw_;
  PortablePluginInterface* plugin_interface_;
  NaClVideoShare*          untrusted_video_share_;
  HtpHandle                video_handle_;
  size_t                   video_size_;
  bool                     video_enabled_;
  VideoCallbackData*       video_callback_data_;
  nacl_srpc::ScriptableHandle<nacl_srpc::SharedMemory>* video_shared_memory_;
  int                      video_update_mode_;
  PluginWindow*            window_;
};

extern void VideoGlobalLock();
extern void VideoGlobalUnlock();

class VideoScopedGlobalLock {
 public:
  VideoScopedGlobalLock() { VideoGlobalLock(); }
  ~VideoScopedGlobalLock() { VideoGlobalUnlock(); }
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_VIDEO_H_
