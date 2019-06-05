// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_BASE_JSON_JSON_WRITER_H_
#define OSP_BASE_JSON_JSON_WRITER_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "json/writer.h"

namespace Json {
class Value;
}

namespace openscreen {
template <typename T>
class ErrorOr;

class JsonWriter {
 public:
  JsonWriter();

  ErrorOr<std::string> Write(const Json::Value& value);

 private:
  Json::StreamWriterBuilder factory_;
};

}  // namespace openscreen

#endif  // OSP_BASE_JSON_JSON_WRITER_H_
