// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NACL_IRT_IRT_MANIFEST_H_
#define PPAPI_NACL_IRT_IRT_MANIFEST_H_

#include <map>
#include <string>

#include "build/build_config.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace ppapi {

// The implementation of irt_open_resource() based on ManifestService.
// This communicates with the renderer process via Chrome IPC to obtain the
// read-only file descriptor of the resource specified in the manifest file
// with the key |file| in files section. Returns 0 on success, or error number
// on failure. See also irt_open_resource()'s comment.
PPAPI_PROXY_EXPORT int IrtOpenResource(const char* file, int* fd);

#if !defined(OS_NACL_SFI)
PPAPI_PROXY_EXPORT void RegisterPreopenedDescriptorsNonSfi(
    std::map<std::string, int>* key_fd_map);
#endif

}  // namespace ppapi

#endif  // PPAPI_NACL_IRT_IRT_MANIFEST_H_
