// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SCRIPTING_SCRIPTING_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SCRIPTING_SCRIPTING_API_H_

#include <string>

#include "chrome/common/extensions/api/scripting.h"
#include "extensions/browser/extension_function.h"

class GURL;

namespace base {
class ListValue;
}

namespace extensions {

class ScriptingExecuteScriptFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("scripting.executeScript", SCRIPTING_EXECUTESCRIPT)

  ScriptingExecuteScriptFunction();
  ScriptingExecuteScriptFunction(const ScriptingExecuteScriptFunction&) =
      delete;
  ScriptingExecuteScriptFunction& operator=(
      const ScriptingExecuteScriptFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ScriptingExecuteScriptFunction() override;

  // Called when the resource file to be injected has been loaded.
  void DidLoadResource(bool success, std::unique_ptr<std::string> data);

  // Triggers the execution of `code_to_execute` in the appropriate context.
  // Returns true on success; on failure, populates `error`.
  bool Execute(std::string code_to_execute,
               GURL script_url,
               std::string* error);

  // Invoked when script execution is complete.
  void OnScriptExecuted(const std::string& error,
                        const GURL& frame_url,
                        const base::ListValue& result);

  api::scripting::ScriptInjection injection_;
};

class ScriptingInsertCSSFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("scripting.insertCSS", SCRIPTING_INSERTCSS)

  ScriptingInsertCSSFunction();
  ScriptingInsertCSSFunction(const ScriptingInsertCSSFunction&) = delete;
  ScriptingInsertCSSFunction& operator=(const ScriptingInsertCSSFunction&) =
      delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ScriptingInsertCSSFunction() override;

  // Called when the resource file to be injected has been loaded.
  void DidLoadResource(bool success, std::unique_ptr<std::string> data);

  // Triggers the execution of `code_to_execute` in the appropriate context.
  // Returns true on success; on failure, populates `error`.
  bool Execute(std::string code_to_execute,
               GURL script_url,
               std::string* error);

  // Called when the CSS insertion is complete.
  void OnCSSInserted(const std::string& error,
                     const GURL& frame_url,
                     const base::ListValue& result);

  api::scripting::CSSInjection injection_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SCRIPTING_SCRIPTING_API_H_
