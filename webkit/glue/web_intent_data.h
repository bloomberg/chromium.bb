// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEB_INTENT_DATA_H_
#define WEBKIT_GLUE_WEB_INTENT_DATA_H_

#include <map>
#include <vector>

#include "base/file_path.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webkit_glue_export.h"

namespace WebKit {
class WebIntent;
}

namespace webkit_glue {

// Representation of the Web Intent data being initiated or delivered.
struct WEBKIT_GLUE_EXPORT WebIntentData {
  // The action of the intent.
  string16 action;
  // The MIME type of data in this intent payload.
  string16 type;
  // The serialized representation of the payload data. Wire format is from
  // WebSerializedScriptValue.
  string16 data;
  // Any extra key-value pair metadata. (Not serialized.)
  std::map<string16, string16> extra_data;

  // Set to the service page if this intent data is from an explicit intent
  // invocation. |service.is_valid()| will be false otherwise.
  GURL service;

  // String payload data.
  string16 unserialized_data;

  // The global message port IDs of any transferred MessagePorts.
  std::vector<int> message_port_ids;

  // The file of a payload blob. Together with |blob_length|, suitable
  // arguments to WebBlob::createFromFile.
  FilePath blob_file;
  // Length of the blob.
  int64 blob_length;

  // These enum values indicate which payload data type should be used.
  enum DataType {
    SERIALIZED = 0,    // The payload is serialized in |data|.
    UNSERIALIZED = 1,  // The payload is unserialized in |unserialized_data|.
    BLOB = 2           // The payload is a blob
  };
  // Which data payload to use when delivering the intent.
  DataType data_type;

  WebIntentData();

  // NOTE! Constructors do not initialize message_port_ids. Caller must do this.

  WebIntentData(const WebKit::WebIntent& intent);
  WebIntentData(const string16& action_in,
                const string16& type_in,
                const string16& unserialized_data_in);
  WebIntentData(const string16& action_in,
                const string16& type_in,
                const FilePath& blob_file_in,
                int64 blob_length_in);
  ~WebIntentData();
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEB_INTENT_DATA_H_
