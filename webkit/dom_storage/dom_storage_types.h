// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_TYPES_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_TYPES_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/nullable_string16.h"
#include "base/string16.h"

// Note: This flag should remain undefined until we're
// ready to use the new backend in chrome and test_shell/DRT.
// This allows us to commit changes behind the flag incrementally
// on trunk, and to enable it in interim snapshots for try runs,
// but to not affect the real build until its ready.
#undef ENABLE_NEW_DOM_STORAGE_BACKEND

namespace dom_storage {

// The quota for each storage area. Suggested by the spec.
const size_t kPerAreaQuota = 5 * 1024 * 1024;

// Value to indicate the localstorage namespace vs non-zero
// values for sessionstorage namespaces.
const int64 kLocalStorageNamespaceId = 0;

// Value to indicate an area that not be opened.
const int kInvalidAreaId = -1;

typedef std::map<string16, NullableString16> ValuesMap;

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_TYPES_H_
