// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "webkit/glue/webkit_glue_export.h"

namespace WebKit {
class WebIDBKey;
class WebIDBKeyPath;
class WebSerializedScriptValue;
}

namespace webkit_glue {

// Warning: this method holds a V8 lock, it should only be called within a
// sandbox.
WEBKIT_GLUE_EXPORT void IDBKeysFromValuesAndKeyPath(
    const std::vector<WebKit::WebSerializedScriptValue>&
        serialized_script_values,
    const WebKit::WebIDBKeyPath& idb_key_path,
    std::vector<WebKit::WebIDBKey>* values);

WEBKIT_GLUE_EXPORT WebKit::WebSerializedScriptValue InjectIDBKey(
    const WebKit::WebIDBKey& key,
    const WebKit::WebSerializedScriptValue& value,
    const WebKit::WebIDBKeyPath& idb_key_path);

}  // namespace webkit_glue
