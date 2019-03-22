// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/engine_resources.h"

namespace chrome_cleaner {

base::string16 GetTestStubFileName(Engine::Name engine) {
  return base::string16();
}

std::string GetEngineVersion(Engine::Name engine) {
  return std::string();
}

int GetProtectedFilesDigestResourceId() {
  return 0;
}

std::unordered_map<base::string16, int> GetEmbeddedLibraryResourceIds(
    Engine::Name engine) {
  return {};
}

int GetLibrariesDigestResourcesId(Engine::Name engine) {
  return 0;
}

std::set<base::string16> GetLibrariesToLoad(Engine::Name engine) {
  return {};
}

std::unordered_map<base::string16, base::string16> GetLibraryTestReplacements(
    Engine::Name engine) {
  return {};
}

std::vector<base::string16> GetDLLNames(Engine::Name engine) {
  return {};
}

}  // namespace chrome_cleaner
