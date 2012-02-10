// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains (de)serialization (or if you like python, pickling)
// methods for various objects that we want to persist.
// In serialization, we write an object's state to a string in some opaque
// format.  Deserialization reconstructs the object's state from such a string.

#ifndef WEBKIT_GLUE_GLUE_SERIALIZE_H_
#define WEBKIT_GLUE_GLUE_SERIALIZE_H_

#include <string>
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

// HistoryItem serialization.
WEBKIT_GLUE_EXPORT std::string HistoryItemToString(
    const WebKit::WebHistoryItem& item);
WEBKIT_GLUE_EXPORT WebKit::WebHistoryItem HistoryItemFromString(
    const std::string& serialized_item);

// For testing purposes only.
WEBKIT_GLUE_EXPORT void HistoryItemToVersionedString(
    const WebKit::WebHistoryItem& item,
    int version,
    std::string* serialized_item);

}  // namespace webkit_glue

#endif  // #ifndef WEBKIT_GLUE_GLUE_SERIALIZE_H_
