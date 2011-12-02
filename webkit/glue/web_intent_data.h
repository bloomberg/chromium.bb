// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEB_INTENT_DATA_H_
#define WEBKIT_GLUE_WEB_INTENT_DATA_H_

#include "base/string16.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

// Representation of the Web Intent data being initiated or delivered.
struct WEBKIT_GLUE_EXPORT WebIntentData {
  // The action of the intent.
  string16 action;
  // The MIME type of data in this intent payload.
  string16 type;
  // The representation of the payload data. Wire format is from
  // SerializedScriptObject.
  string16 data;

  WebIntentData();
  WebIntentData(const WebIntentData& other);
  ~WebIntentData();
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEB_INTENT_DATA_H_
