// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/idb_bindings.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyPath.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace webkit_glue {

using WebKit::WebIDBKey;
using WebKit::WebIDBKeyPath;
using WebKit::WebSerializedScriptValue;
using WebKit::WebString;

void IDBKeysFromValuesAndKeyPath(
    const std::vector<WebSerializedScriptValue>& serialized_script_values,
    const WebIDBKeyPath& idb_key_path,
    std::vector<WebIDBKey>* values) {
  for (std::vector<WebSerializedScriptValue>::const_iterator i =
           serialized_script_values.begin();
       i != serialized_script_values.end(); ++i) {
    values->push_back(
        WebIDBKey::createFromValueAndKeyPath(*i, idb_key_path));
  }
}

WebSerializedScriptValue InjectIDBKey(
    const WebIDBKey& key,
    const WebSerializedScriptValue& value,
    const WebIDBKeyPath& idb_key_path) {
  return WebIDBKey::injectIDBKeyIntoSerializedValue(
      key, value, idb_key_path);
}

}  // namespace webkit_glue
