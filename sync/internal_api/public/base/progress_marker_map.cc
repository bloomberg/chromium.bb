// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/progress_marker_map.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/values.h"

namespace syncer {

scoped_ptr<DictionaryValue> ProgressMarkerMapToValue(
    const ProgressMarkerMap& marker_map) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  for (ProgressMarkerMap::const_iterator it = marker_map.begin();
       it != marker_map.end(); ++it) {
    std::string printable_payload;
    base::JsonDoubleQuote(it->second,
                          false /* put_in_quotes */,
                          &printable_payload);
    value->SetString(ModelTypeToString(it->first), printable_payload);
  }
  return value.Pass();
}

} // namespace syncer
