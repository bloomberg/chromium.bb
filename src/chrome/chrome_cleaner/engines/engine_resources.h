// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_ENGINE_RESOURCES_H_
#define CHROME_CHROME_CLEANER_ENGINES_ENGINE_RESOURCES_H_

#include <string>

#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"

namespace chrome_cleaner {

// Returns string representation of the engine's version or an empty string if
// not available.
std::string GetEngineVersion(Engine::Name engine);

// Returns ID of the "TEXT" resource that contains serialized FileDigests
// message (see file_digest.proto) of protected files. Returns zero if not
// available.
int GetProtectedFilesDigestResourceId();

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_ENGINE_RESOURCES_H_
