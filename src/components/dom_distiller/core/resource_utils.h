// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_RESOURCE_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_RESOURCE_UTILS_H_

#include <string>

namespace dom_distiller {

// Retrieves (and uncompresses if needed) a given resource as a string.
std::string GetResourceFromIdAsString(int resource_id);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_RESOURCE_UTILS_H_
