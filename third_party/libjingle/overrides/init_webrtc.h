// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBJINGLE_OVERRIDES_INIT_WEBRTC_H_
#define THIRD_PARTY_LIBJINGLE_OVERRIDES_INIT_WEBRTC_H_

#include "allocator_shim/allocator_stub.h"

class CommandLine;

namespace cricket {
class MediaEngineInterface;
class WebRtcVideoDecoderFactory;
}  // namespace cricket

namespace webrtc {
class AudioDeviceModule;
}  // namespace webrtc

typedef cricket::MediaEngineInterface* (*CreateWebRtcMediaEngineFunction)(
    webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc,
    cricket::WebRtcVideoDecoderFactory* decoder_factory);

typedef void (*DestroyWebRtcMediaEngineFunction)(
    cricket::MediaEngineInterface* media_engine);

// A typedef for the main initialize function in libpeerconnection.
// This will initialize logging in the module with the proper arguments
// as well as provide pointers back to a couple webrtc factory functions.
// The reason we get pointers to these functions this way is to avoid having
// to go through GetProcAddress et al and rely on specific name mangling.
typedef bool (*InitializeModuleFunction)(
    const CommandLine& command_line,
#if !defined(OS_MACOSX)
    AllocateFunction alloc,
    DellocateFunction dealloc,
#endif
    CreateWebRtcMediaEngineFunction* create_media_engine,
    DestroyWebRtcMediaEngineFunction* destroy_media_engine);

// Load and initialize the shared WebRTC module (libpeerconnection).
// Call this explicitly to load and initialize the WebRTC module (e.g. before
// initializing the sandbox in Chrome).
// If not called explicitly, this function will still be called from the main
// CreateWebRtcMediaEngine factory function the first time it is called.
bool InitializeWebRtcModule();

#endif // THIRD_PARTY_LIBJINGLE_OVERRIDES_INIT_WEBRTC_H_
