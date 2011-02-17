// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

namespace WebKit {
class WebIDBKey;
class WebSerializedScriptValue;
}

namespace webkit_glue {

// Warning: this method holds a V8 lock, it should only be called within a
// sandbox.
bool IDBKeysFromValuesAndKeyPath(
    const std::vector<WebKit::WebSerializedScriptValue>&
        serialized_script_values,
    const string16& idb_key_path,
    std::vector<WebKit::WebIDBKey>* values);

WebKit::WebSerializedScriptValue InjectIDBKey(
    const WebKit::WebIDBKey& key,
    const WebKit::WebSerializedScriptValue& value,
    const string16& idb_key_path);

}  // namespace webkit_glue
