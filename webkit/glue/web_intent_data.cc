// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_intent_data.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMessagePortChannel.h"

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
                             const string16& type_in)
    : action(action_in),
      type(type_in),
      blob_length(0),
      data_type(MIME_TYPE) {
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
                             const std::string& root_name_in,
                             const std::string& filesystem_id_in)
    : action(action_in),
      type(type_in),
      blob_length(0),
      root_name(root_name_in),
      filesystem_id(filesystem_id_in),
      data_type(FILESYSTEM) {
}

WebIntentData::WebIntentData(const WebIntentData& intent_data)
    : action(intent_data.action),
      type(intent_data.type),
      data(intent_data.data),
      extra_data(intent_data.extra_data),
      service(intent_data.service),
      suggestions(intent_data.suggestions),
      unserialized_data(intent_data.unserialized_data),
      message_port_ids(intent_data.message_port_ids),
      blob_file(intent_data.blob_file),
      blob_length(intent_data.blob_length),
      root_name(intent_data.root_name),
      filesystem_id(intent_data.filesystem_id),
      data_type(intent_data.data_type) {
  for (size_t i = 0; i < intent_data.mime_data.GetSize(); ++i) {
    const DictionaryValue* dict = NULL;
    if (!intent_data.mime_data.GetDictionary(i, &dict)) continue;
    mime_data.Append(dict->DeepCopy());
  }
}

}  // namespace webkit_glue
