// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/resource_metadata.h"

namespace viz {

ResourceMetadata::ResourceMetadata() = default;

ResourceMetadata::ResourceMetadata(const ResourceMetadata& other) = default;

ResourceMetadata::~ResourceMetadata() = default;

ResourceMetadata& ResourceMetadata::operator=(const ResourceMetadata& other) =
    default;

}  // namespace viz
