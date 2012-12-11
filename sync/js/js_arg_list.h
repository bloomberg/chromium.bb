// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_JS_JS_ARG_LIST_H_
#define SYNC_JS_JS_ARG_LIST_H_

// See README.js for design comments.

#include <string>

#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/immutable.h"

namespace syncer {

// A thin wrapper around Immutable<ListValue>.  Used for passing
// around argument lists to different threads.
class SYNC_EXPORT JsArgList {
 public:
  // Uses an empty argument list.
  JsArgList();

  // Takes over the data in |args|, leaving |args| empty.
  explicit JsArgList(ListValue* args);

  ~JsArgList();

  const ListValue& Get() const;

  std::string ToString() const;

  // Copy constructor and assignment operator welcome.

 private:
  typedef Immutable<ListValue, HasSwapMemFnByPtr<ListValue> >
      ImmutableListValue;
  ImmutableListValue args_;
};

}  // namespace syncer

#endif  // SYNC_JS_JS_ARG_LIST_H_
