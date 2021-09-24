// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/chromeos_extensions.h"

#include "third_party/blink/renderer/bindings/extensions_chromeos/v8/v8_chrome_os.h"
#include "third_party/blink/renderer/platform/bindings/extensions_registry.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"

namespace blink {

namespace {
void ChromeOSExposedConstructCallback(
    v8::Local<v8::Name> v8_property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  bindings::V8SetReturnValue(info, V8ChromeOS::GetWrapperTypeInfo(),
                             bindings::V8ReturnValue::kNamespaceObject);
}

void InstallChromeOSExtensions(ScriptState* script_state) {
  auto global_proxy = script_state->GetContext()->Global();

  global_proxy
      ->SetLazyDataProperty(
          script_state->GetContext(),
          V8String(script_state->GetIsolate(),
                   V8ChromeOS::GetWrapperTypeInfo()->interface_name),
          ChromeOSExposedConstructCallback, v8::Local<v8::Value>(),
          v8::DontEnum, v8::SideEffectType::kHasNoSideEffect)
      .ToChecked();
}

}  // namespace

void ChromeOSExtensions::Initialize() {
  ExtensionsRegistry::GetInstance().RegisterBlinkExtensionInstallCallback(
      &InstallChromeOSExtensions);
}

}  // namespace blink
