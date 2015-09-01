// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/proxy.h"

namespace sql {

int sqlite3_create_function_v2(
    sqlite3 *db,
    const char *zFunctionName,
    int nArg,
    int eTextRep,
    void *pApp,
    void (*xFunc)(sqlite3_context*,int,sqlite3_value**),
    void (*xStep)(sqlite3_context*,int,sqlite3_value**),
    void (*xFinal)(sqlite3_context*),
    void (*xDestroy)(void*)) {
  return ::sqlite3_create_function_v2(
      db, zFunctionName, nArg, eTextRep, pApp,
      xFunc, xStep, xFinal, xDestroy);
}

void *sqlite3_commit_hook(sqlite3* db, int(*func)(void*), void* arg) {
  return ::sqlite3_commit_hook(db, func, arg);
}

}  // namespace sql
