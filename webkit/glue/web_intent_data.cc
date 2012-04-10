// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_intent_data.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntent.h"

namespace webkit_glue {

WebIntentData::WebIntentData()
    : blob_length(0),
      data_type(SERIALIZED) {
}

WebIntentData::~WebIntentData() {
}

WebIntentData::WebIntentData(const WebKit::WebIntent& intent)
    : action(intent.action()),
      type(intent.type()),
      data(intent.data()),
      blob_length(0),
      data_type(SERIALIZED) {
}

WebIntentData::WebIntentData(const string16& action_in,
                             const string16& type_in,
                             const string16& unserialized_data_in)
    : action(action_in),
      type(type_in),
      unserialized_data(unserialized_data_in),
      blob_length(0),
      data_type(UNSERIALIZED) {
}

WebIntentData::WebIntentData(const string16& action_in,
                             const string16& type_in,
                             const FilePath& blob_file_in,
                             int64 blob_length_in)
    : action(action_in),
      type(type_in),
      blob_file(blob_file_in),
      blob_length(blob_length_in),
      data_type(BLOB) {
}

}  // namespace webkit_glue
