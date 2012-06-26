// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_intent_data.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMessagePortChannel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

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
      service(intent.service()),
      blob_length(0),
      data_type(SERIALIZED) {
  const WebVector<WebString>& names = intent.extrasNames();
  for (size_t i = 0; i < names.size(); ++i) {
    extra_data[names[i]] = intent.extrasValue(names[i]);
  }

  const WebVector<WebURL>& clientSuggestions = intent.suggestions();
  for (size_t i = 0; i < clientSuggestions.size(); ++i)
    suggestions.push_back(clientSuggestions[i]);
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

WebIntentData::WebIntentData(const string16& action_in,
                             const string16& type_in,
                             const FilePath& root_path_in,
                             const std::string& filesystem_id_in)
    : action(action_in),
      type(type_in),
      blob_length(0),
      root_path(root_path_in),
      filesystem_id(filesystem_id_in),
      data_type(FILESYSTEM) {
}

}  // namespace webkit_glue
