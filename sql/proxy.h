// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_PROXY_H_
#define SQL_PROXY_H_

#include "sql/sql_export.h"
#include "third_party/sqlite/sqlite3.h"

// TODO(shess): third_party/sqlite does not track component build correctly, so
// each shared library gets a private copy of everything, so sqlite3_* calls
// outside of the main sql/ component don't work right.  Hack around this by
// adding pass-through functions while I land a separate fix for the component
// issue.

// This is only required for tests - if these abilities are desired for
// production code, they should probably do obvious things like live in
// sql::Connection and use C++ wrappers.

// http://crbug.com/489444

namespace sql {

SQL_EXPORT int sqlite3_create_function_v2(
    sqlite3 *db,
    const char *zFunctionName,
    int nArg,
    int eTextRep,
    void *pApp,
    void (*xFunc)(sqlite3_context*,int,sqlite3_value**),
    void (*xStep)(sqlite3_context*,int,sqlite3_value**),
    void (*xFinal)(sqlite3_context*),
    void (*xDestroy)(void*));
SQL_EXPORT void *sqlite3_commit_hook(sqlite3*, int(*)(void*), void*);

}  // namespace sql

#endif  // SQL_PROXY_H_
