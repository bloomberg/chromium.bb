// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_WIDEVINE_LOADER_H_
#define CEF_LIBCEF_COMMON_WIDEVINE_LOADER_H_
#pragma once

#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)

#include <vector>

#include "include/cef_web_plugin.h"

#include "base/lazy_instance.h"

namespace content {
struct CdmInfo;
}

namespace media {
struct CdmHostFilePath;
}

class CefWidevineLoader {
 public:
  // Returns the singleton instance of this object.
  static CefWidevineLoader* GetInstance();

  // Load the Widevine CDM. May be called before or after context creation. See
  // comments in cef_web_plugin.h.
  void LoadWidevineCdm(const base::FilePath& path,
                       CefRefPtr<CefRegisterCdmCallback> callback);

  // Plugin registration is triggered here if LoadWidevineCdm() was called
  // before context creation.
  void OnContextInitialized();

#if defined(OS_LINUX)
  // The zygote process which is used when the sandbox is enabled on Linux
  // requires early loading of CDM modules. Other processes will receive
  // load notification in the usual way.
  // Called from AlloyContentClient::AddContentDecryptionModules.
  static void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms,
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths);

  const base::FilePath& path() { return path_; }
#endif

 private:
  friend struct base::LazyInstanceTraitsBase<CefWidevineLoader>;

  // Members are only accessed before context initialization or on the UI
  // thread.
  bool load_pending_ = false;
  base::FilePath path_;
  CefRefPtr<CefRegisterCdmCallback> callback_;

  CefWidevineLoader();
  ~CefWidevineLoader();
};

#endif  // BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)

#endif  // CEF_LIBCEF_COMMON_WIDEVINE_LOADER_H_
