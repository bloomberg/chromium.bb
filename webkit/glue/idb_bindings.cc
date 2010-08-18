// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/idb_bindings.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBKeyPath.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "v8/include/v8.h"

namespace webkit_glue {

using WebKit::WebIDBKey;
using WebKit::WebIDBKeyPath;
using WebKit::WebSerializedScriptValue;

namespace {

class LocalContext {
 public:
  LocalContext()
      : context_(v8::Context::New()) {
    context_->Enter();
  }

  virtual ~LocalContext() {
    context_->Exit();
    context_.Dispose();
  }

 private:
  v8::Locker lock_;
  v8::HandleScope scope_;
  v8::Persistent<v8::Context> context_;

  DISALLOW_COPY_AND_ASSIGN(LocalContext);
};

}  // namespace

bool IDBKeysFromValuesAndKeyPath(
    const std::vector<WebSerializedScriptValue>& serialized_script_values,
    const string16& idb_key_path,
    std::vector<WebIDBKey>* values) {
  LocalContext env;
  WebIDBKeyPath web_idb_key_path = WebIDBKeyPath::create(idb_key_path);
  bool error = web_idb_key_path.parseError() != 0;
  // TODO(bulach): what to do when we have a parse error? For now, setting
  // all values back as invalid and returning a boolean.
  for (std::vector<WebSerializedScriptValue>::const_iterator i =
           serialized_script_values.begin();
       i != serialized_script_values.end(); ++i) {
    if (error) {
      values->push_back(WebIDBKey::createInvalid());
    } else {
      values->push_back(
          WebIDBKey::createFromValueAndKeyPath(*i, web_idb_key_path));
    }
  }
  return error;
}

}  // namespace webkit_glue
