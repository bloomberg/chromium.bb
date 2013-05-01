// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "allocator_shim/allocator_stub.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "talk/base/basictypes.h"
#include "talk/media/webrtc/webrtcmediaengine.h"

#if !defined(LIBPEERCONNECTION_IMPLEMENTATION) || defined(LIBPEERCONNECTION_LIB)
#error "Only compile the allocator proxy with the shared_library implementation"
#endif

#if defined(OS_WIN)
#define ALLOC_EXPORT __declspec(dllexport)
#else
#define ALLOC_EXPORT __attribute__((visibility("default")))
#endif

typedef cricket::MediaEngineInterface* (*CreateWebRtcMediaEngineFunction)(
    webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc,
    cricket::WebRtcVideoDecoderFactory* decoder_factory);
typedef void (*DestroyWebRtcMediaEngineFunction)(
    cricket::MediaEngineInterface* media_engine);

#if !defined(OS_MACOSX)
// These are used by our new/delete overrides in
// allocator_shim/allocator_proxy.cc
AllocateFunction g_alloc = NULL;
DellocateFunction g_dealloc = NULL;
#endif

// Forward declare of the libjingle internal factory and destroy methods for the
// WebRTC media engine.
cricket::MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm, webrtc::AudioDeviceModule* adm_sc,
    cricket::WebRtcVideoDecoderFactory* decoder_factory);

void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine);

extern "C" {

// Initialize logging, set the forward allocator functions (not on mac), and
// return pointers to libjingle's WebRTC factory methods.
// Called from init_webrtc.cc.
ALLOC_EXPORT
bool InitializeModule(const CommandLine& command_line,
#if !defined(OS_MACOSX)
                      AllocateFunction alloc,
                      DellocateFunction dealloc,
#endif
                      CreateWebRtcMediaEngineFunction* create_media_engine,
                      DestroyWebRtcMediaEngineFunction* destroy_media_engine) {
#if !defined(OS_MACOSX)
  g_alloc = alloc;
  g_dealloc = dealloc;
#endif

  *create_media_engine = &CreateWebRtcMediaEngine;
  *destroy_media_engine = &DestroyWebRtcMediaEngine;

  if (CommandLine::Init(0, NULL)) {
#if !defined(OS_WIN)
    // This is not needed on Windows since CommandLine::Init has already
    // done the equivalent thing via the GetCommandLine() API.
    CommandLine::ForCurrentProcess()->AppendArguments(command_line, true);
#endif
    logging::InitLogging(
        FILE_PATH_LITERAL("libpeerconnection.log"),
        logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
        logging::LOCK_LOG_FILE,
        logging::APPEND_TO_OLD_LOG_FILE,
        logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  }

  return true;
}
}  // extern "C"
