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

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/npapi_native.h"
#include "native_client/src/trusted/plugin/srpc/srpc.h"
#include "native_client/src/trusted/plugin/srpc/portable_handle.h"
#include "native_client/src/trusted/plugin/srpc/video.h"

// This file provides empty implementation for video functions used in NaCl code
// These functions should not be used when running as part of Chrome, but to
// avoid multiple changes in many places in the code, we just provide this
// dummy implementation.

namespace nacl_srpc {
  class Plugin;
}

namespace nacl {

void VideoGlobalLock() {
  return;
}

void VideoGlobalUnlock() {
  return;
}

void VideoMap::Redraw() {
  return;
}

void VideoMap::RedrawAsync(void *platform_parm) {
  return;
}

// assumes event is already normalized to NaClMultimediaEvent
int VideoMap::EventQueuePut(union NaClMultimediaEvent *event) {
  return -1;
}

// tests event queue to see if it is full
int VideoMap::EventQueueIsFull() {
  return 1;
}

// Gets the last recorded mouse motion (position)
// note: outputs normalized for NaCl events
void VideoMap::GetMotion(uint16_t *x, uint16_t *y) {
  return;
}

// Sets the mouse motion (position)
// note: the inputs must be normalized for NaCl events
void VideoMap::SetMotion(uint16_t x, uint16_t y, int last_valid) {
  return;
}

// Gets the relative mouse motion and updates position
// note: the inputs must be normalized for NaCl events
void VideoMap::GetRelativeMotion(uint16_t x,
                                 uint16_t y,
                                 int16_t *rel_x,
                                 int16_t *rel_y) {
  return;
}

// Gets the current mouse button state
// note: output is normalized for NaCl events
int VideoMap::GetButton() {
  return 0;
}

// Modifies the mouse button state
// note: input must be normalized for NaCl events
void VideoMap::SetButton(int button, int state) {
  return;
}

// Modifies the keyboard modifier state (shift, ctrl, alt, etc)
// note: input must be normalized for NaCl events
void VideoMap::SetKeyMod(int nsym, int state) {
  return;
}

// Gets the current keyboard modifier state
// note: output is normalized for NaCl Events
int VideoMap::GetKeyMod() {
  return 0;
}

// handle video map specific NPAPI SetWindow
bool VideoMap::SetWindow(PluginWindow *window) {
  // TODO(gregoryd): do something with this information
  // or remove this file.
  return true;
}

int16_t VideoMap::HandleEvent(void *param) {
  return 0;
}

nacl_srpc::ScriptableHandle<nacl_srpc::SharedMemory>*
    VideoMap::VideoSharedMemorySetup() {
  return NULL;
}

// Note: Graphics functionality similar to npapi_bridge.
// TODO(sehr): Make only one copy of the graphics code.
void VideoMap::Invalidate() {
  return;
}

VideoCallbackData* VideoMap::InitCallbackData(NaClHandle h,
                                            PortablePluginInterface *p,
                                            nacl_srpc::MultimediaSocket *msp) {
  return NULL;
}

// static method
VideoCallbackData* VideoMap::ReleaseCallbackData(VideoCallbackData* vcd) {
  return NULL;
}

// static method
// normally, the Release version is preferred, which reference counts.
// in some cases we may just need to forcefully delete the callback data,
// and ignore the refcount.
void VideoMap::ForceDeleteCallbackData(VideoCallbackData* vcd) {
  return;
}

// opens shared memory map into untrusted space
// returns 0 success, -1 failure
int VideoMap::InitializeSharedMemory(PluginWindow *window) {
  return -1;
}

// this is the draw method the upcall thread should invoke
void VideoMap::RequestRedraw() {
  return;
}

VideoMap::VideoMap(PortablePluginInterface *plugin_interface) {
  plugin_interface_ = plugin_interface;
}

VideoMap::~VideoMap() {
}

}  // namespace nacl
