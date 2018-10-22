// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_I18N_HOOKS_DELEGATE_H_
#define EXTENSIONS_RENDERER_I18N_HOOKS_DELEGATE_H_

#include <vector>

#include "base/macros.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8.h"

namespace extensions {
class ScriptContext;

// Custom native hooks for the i18n API.
class I18nHooksDelegate : public APIBindingHooksDelegate {
 public:
  I18nHooksDelegate();
  ~I18nHooksDelegate() override;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      std::vector<v8::Local<v8::Value>>* arguments,
      const APITypeReferenceMap& refs) override;

 private:
  // Method handlers:
  APIBindingHooks::RequestResult HandleGetMessage(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& parsed_arguments);
  APIBindingHooks::RequestResult HandleGetUILanguage(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& parsed_arguments);
  APIBindingHooks::RequestResult HandleDetectLanguage(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& parsed_arguments);

  DISALLOW_COPY_AND_ASSIGN(I18nHooksDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_I18N_HOOKS_DELEGATE_H_
