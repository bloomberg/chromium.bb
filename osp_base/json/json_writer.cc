// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/json/json_writer.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "json/value.h"
#include "osp_base/error.h"
#include "platform/api/logging.h"

namespace openscreen {
JsonWriter::JsonWriter() {
#ifndef _DEBUG
  // Default is to "pretty print" the output JSON in a human readable
  // format. On non-debug builds, we can remove pretty printing by simply
  // getting rid of all indentation.
  factory_["indentation"] = "";
#endif
}

ErrorOr<std::string> JsonWriter::Write(const Json::Value& value) {
  if (value.empty()) {
    return ErrorOr<std::string>(Error::Code::kJsonWriteError, "Empty value");
  }

  std::unique_ptr<Json::StreamWriter> const writer(factory_.newStreamWriter());
  std::stringstream stream;
  writer->write(value, &stream);
  stream << std::endl;

  if (!stream) {
    // Note: jsoncpp doesn't give us more information about what actually
    // went wrong, just says to "check the stream". However, failures on
    // the stream should be rare, as we do not throw any errors in the jsoncpp
    // library.
    return ErrorOr<std::string>(Error::Code::kJsonWriteError, "Invalid stream");
  }

  return stream.str();
}
}  // namespace openscreen
