// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/npapi/video.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"

// This file provides empty implementation for video functions used in NaCl code
// These functions should not be used when running as part of Chrome, but to
// avoid multiple changes in many places in the code, we just provide this
// dummy implementation.

namespace plugin {

class Plugin;

void VideoMap::Redraw() {
  return;
}

void VideoMap::RedrawAsync(void* platform_parm) {
  return;
}

// assumes event is already normalized to NaClMultimediaEvent
int VideoMap::EventQueuePut(union NaClMultimediaEvent* event) {
  return -1;
}

// tests event queue to see if it is full
int VideoMap::EventQueueIsFull() {
  return 1;
}

// Gets the last recorded mouse motion (position)
// note: outputs normalized for NaCl events
void VideoMap::GetMotion(uint16_t* x, uint16_t* y) {
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
                                 int16_t* rel_x,
                                 int16_t* rel_y) {
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
bool VideoMap::SetWindow(PluginWindow* window) {
  // TODO(gregoryd): do something with this information
  // or remove this file.
  return true;
}

int16_t VideoMap::HandleEvent(void* param) {
  return 0;
}

ScriptableHandle* VideoMap::VideoSharedMemorySetup() {
  return NULL;
}

void VideoMap::Invalidate() {
  return;
}

VideoCallbackData* VideoMap::InitCallbackData(nacl::DescWrapper* desc,
                                              Plugin* p,
                                              MultimediaSocket* msp) {
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
int VideoMap::InitializeSharedMemory(PluginWindow* window) {
  return -1;
}

// this is the draw method the upcall thread should invoke
void VideoMap::RequestRedraw() {
  return;
}

VideoMap::VideoMap(Plugin* plugin) {
}

VideoMap::~VideoMap() {
}

}  // namespace plugin
