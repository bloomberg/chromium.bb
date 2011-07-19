// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/npruntime_util.h"

#include "base/pickle.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"

using WebKit::WebBindings;

namespace webkit_glue {

bool SerializeNPIdentifier(NPIdentifier identifier, Pickle* pickle) {
  const NPUTF8* string;
  int32_t number;
  bool is_string;
  WebBindings::extractIdentifierData(identifier, string, number, is_string);

  if (!pickle->WriteBool(is_string))
    return false;
  if (is_string) {
    // Write the null byte for efficiency on the other end.
    return pickle->WriteData(string, strlen(string) + 1);
  }
  return pickle->WriteInt(number);
}

bool DeserializeNPIdentifier(const Pickle& pickle, void** pickle_iter,
                             NPIdentifier* identifier) {
  bool is_string;
  if (!pickle.ReadBool(pickle_iter, &is_string))
    return false;

  if (is_string) {
    const char* data;
    int data_len;
    if (!pickle.ReadData(pickle_iter, &data, &data_len))
      return false;
    DCHECK_EQ((static_cast<size_t>(data_len)), strlen(data) + 1);
    *identifier = WebBindings::getStringIdentifier(data);
  } else {
    int number;
    if (!pickle.ReadInt(pickle_iter, &number))
      return false;
    *identifier = WebBindings::getIntIdentifier(number);
  }
  return true;
}

}  // namespace webkit_glue
