// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init_webrtc.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "talk/base/basictypes.h"

#if defined(LIBPEERCONNECTION_LIB)

// libpeerconnection is being compiled as a static lib.  In this case
// we don't need to do any initializing but to keep things simple we
// provide an empty intialization routine so that this #ifdef doesn't
// have to be in other places.
bool InitializeWebRtcModule() {
  return true;
}

#else  // !LIBPEERCONNECTION_LIB

// When being compiled as a shared library, we need to bridge the gap between
// the current module and the libpeerconnection module, so things get a tad
// more complicated.

// Global function pointers to the factory functions in the shared library.
CreateWebRtcMediaEngineFunction g_create_webrtc_media_engine = NULL;
DestroyWebRtcMediaEngineFunction g_destroy_webrtc_media_engine = NULL;

// Returns the full or relative path to the libpeerconnection module depending
// on what platform we're on.
static base::FilePath GetLibPeerConnectionPath() {
#if defined(OS_WIN)
  base::FilePath path(FILE_PATH_LITERAL("libpeerconnection.dll"));
#elif defined(OS_MACOSX)
  // Simulate '@loader_path/Libraries'.
  base::FilePath path;
  CHECK(PathService::Get(base::DIR_MODULE, &path));
  path = path.Append(FILE_PATH_LITERAL("Libraries"))
             .Append(FILE_PATH_LITERAL("libpeerconnection.so"));
#else
  base::FilePath path;
  CHECK(PathService::Get(base::DIR_MODULE, &path));
  path = path.Append(FILE_PATH_LITERAL("lib"))
             .Append(FILE_PATH_LITERAL("libpeerconnection.so"));
#endif
  return path;
}

bool InitializeWebRtcModule() {
  if (g_create_webrtc_media_engine)
    return true;  // InitializeWebRtcModule has already been called.

  base::FilePath path(GetLibPeerConnectionPath());
  DVLOG(1) << "Loading WebRTC module: " << path.value();

  std::string error;
  static base::NativeLibrary lib =
      base::LoadNativeLibrary(path, &error);
  CHECK(lib);

  InitializeModuleFunction initialize_module =
      reinterpret_cast<InitializeModuleFunction>(
          base::GetFunctionPointerFromNativeLibrary(
              lib, "InitializeModule"));

  // Initialize the proxy by supplying it with a pointer to our
  // allocator/deallocator routines.
  // On mac we use malloc zones, which are global, so we provide NULLs for
  // the alloc/dealloc functions.
  // PS: This function is actually implemented in allocator_proxy.cc with the
  // new/delete overrides.
  return initialize_module(*CommandLine::ForCurrentProcess(),
#if !defined(OS_MACOSX)
      &Allocate, &Dellocate,
#endif
      &g_create_webrtc_media_engine, &g_destroy_webrtc_media_engine);
}

cricket::MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc,
    cricket::WebRtcVideoDecoderFactory* decoder_factory) {
  // For convenience of tests etc, we call InitializeWebRtcModule here.
  // For Chrome however, InitializeWebRtcModule must be called
  // explicitly before the sandbox is initialized.  In that case, this call is
  // effectively a noop.
  InitializeWebRtcModule();
  return g_create_webrtc_media_engine(adm, adm_sc, decoder_factory);
}

void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine) {
  g_destroy_webrtc_media_engine(media_engine);
}

#endif  // LIBPEERCONNECTION_LIB
