// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InstalledScriptsManager_h
#define InstalledScriptsManager_h

#include "platform/network/HTTPHeaderMap.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Vector.h"

namespace blink {

// InstalledScriptsManager provides the scripts of workers that have been
// installed. Currently it is only used for installed service workers.
class InstalledScriptsManager {
 public:
  InstalledScriptsManager() = default;

  struct ScriptData {
    HTTPHeaderMap headers;
    String source_text;
    std::unique_ptr<Vector<char>> meta_data;
  };

  // Used on the main or worker thread. Returns true if the script has been
  // installed.
  virtual bool IsScriptInstalled(const KURL& script_url) const = 0;

  // Used on the worker thread. This is possible to return WTF::nullopt when the
  // script has already been served from this manager (i.e. the same script is
  // read more than once). This can be blocked if the script is not streamed
  // yet.
  virtual Optional<ScriptData> GetScriptData(const KURL& script_url) = 0;
};

}  // namespace blink

#endif  // WorkerInstalledScriptsManager_h
