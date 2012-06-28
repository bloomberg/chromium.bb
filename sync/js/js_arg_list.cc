// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/js/js_arg_list.h"

#include "base/json/json_writer.h"

namespace syncer {

JsArgList::JsArgList() {}

JsArgList::JsArgList(ListValue* args) : args_(args) {}

JsArgList::~JsArgList() {}

const ListValue& JsArgList::Get() const {
  return args_.Get();
}

std::string JsArgList::ToString() const {
  std::string str;
  base::JSONWriter::Write(&Get(), &str);
  return str;
}

}  // namespace syncer
