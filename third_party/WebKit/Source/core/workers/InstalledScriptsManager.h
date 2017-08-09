// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InstalledScriptsManager_h
#define InstalledScriptsManager_h

#include "core/CoreExport.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
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

  class CORE_EXPORT ScriptData {
   public:
    ScriptData() = default;
    ScriptData(const KURL& script_url,
               String source_text,
               std::unique_ptr<Vector<char>> meta_data,
               std::unique_ptr<CrossThreadHTTPHeaderMapData>);
    ScriptData(ScriptData&& other) = default;
    ScriptData& operator=(ScriptData&& other) = default;

    String TakeSourceText() { return std::move(source_text_); }
    std::unique_ptr<Vector<char>> TakeMetaData() {
      return std::move(meta_data_);
    }

    ContentSecurityPolicyResponseHeaders
    GetContentSecurityPolicyResponseHeaders();
    String GetReferrerPolicy();
    std::unique_ptr<Vector<String>> CreateOriginTrialTokens();

   private:
    KURL script_url_;
    String source_text_;
    std::unique_ptr<Vector<char>> meta_data_;
    HTTPHeaderMap headers_;

    DISALLOW_COPY_AND_ASSIGN(ScriptData);
  };

  // Used on the main or worker thread. Returns true if the script has been
  // installed.
  virtual bool IsScriptInstalled(const KURL& script_url) const = 0;

  enum class ScriptStatus { kSuccess, kTaken, kFailed };
  // Used on the worker thread. GetScriptData() can provide a script for the
  // |script_url| only once. When GetScriptData returns
  // - ScriptStatus::kSuccess: the script has been received correctly. Sets the
  //                           script to |out_script_data|.
  // - ScriptStatus::kTaken: the script has been served from this manager
  //                         (i.e. the same script is read more than once).
  //                         |out_script_data| is left as is.
  // - ScriptStatus::kFailed: an error happened while receiving the script from
  //                          the browser process.
  //                         |out_script_data| is left as is.
  // This can block if the script has not been received from the browser process
  // yet.
  virtual ScriptStatus GetScriptData(const KURL& script_url,
                                     ScriptData* out_script_data) = 0;
};

}  // namespace blink

#endif  // InstalledScriptsManager_h
