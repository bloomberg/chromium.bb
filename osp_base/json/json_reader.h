// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_BASE_JSON_JSON_READER_H_
#define OSP_BASE_JSON_JSON_READER_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "json/reader.h"

namespace Json {
class Value;
}

namespace openscreen {
template <typename T>
class ErrorOr;

class JsonReader {
 public:
  JsonReader();

  ErrorOr<Json::Value> Read(absl::string_view document);

 private:
  Json::CharReaderBuilder builder_;
};

}  // namespace openscreen

#endif  // OSP_BASE_JSON_JSON_READER_H_