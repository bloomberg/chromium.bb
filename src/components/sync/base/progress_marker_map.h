// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PROGRESS_MARKER_MAP_H_
#define COMPONENTS_SYNC_BASE_PROGRESS_MARKER_MAP_H_

#include <map>
#include <memory>
#include <string>

#include "components/sync/base/model_type.h"

namespace base {
class DictionaryValue;
}

namespace syncer {

// A container that maps ModelType to serialized
// DataTypeProgressMarkers.
using ProgressMarkerMap = std::map<ModelType, std::string>;

std::unique_ptr<base::DictionaryValue> ProgressMarkerMapToValue(
    const ProgressMarkerMap& marker_map);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PROGRESS_MARKER_MAP_H_
