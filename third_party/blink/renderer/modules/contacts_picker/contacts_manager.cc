// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/contacts_picker/contacts_manager.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

ContactsManager::ContactsManager() = default;
ContactsManager::~ContactsManager() = default;

ScriptPromise ContactsManager::select(ScriptState* script_state,
                                      ContactsSelectOptions* options) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      DOMException::Create(DOMExceptionCode::kNotSupportedError,
                           "ContactsManager::select is not yet implemented"));
}

}  // namespace blink
