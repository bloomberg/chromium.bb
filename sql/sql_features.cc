// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sql_features.h"

namespace sql {

namespace features {

// SQLite databases only use RAM for temporary storage.
//
// Enabling this feature matches the SQLITE_TEMP_STORE=3 build option, which is
// used on Android.
//
// TODO(pwnall): After the memory impact of the config change is assessed, land
//               https://crrev.com/c/1146493 and remove this flag.
const base::Feature kSqlTempStoreMemory{"SqlTempStoreMemory",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

}  // namespace sql
