// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/json_parser/json_parser_impl.h"

#include "base/json/json_reader.h"
#include "base/values.h"

namespace chrome_cleaner {

JsonParserImpl::JsonParserImpl(mojom::JsonParserRequest request,
                               base::OnceClosure connection_error_handler)
    : binding_(this, std::move(request)) {
  binding_.set_connection_error_handler(std::move(connection_error_handler));
}

JsonParserImpl::~JsonParserImpl() = default;

void JsonParserImpl::Parse(const std::string& json, ParseCallback callback) {
  int error_code;
  std::string error;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      json,
      base::JSON_ALLOW_TRAILING_COMMAS | base::JSON_REPLACE_INVALID_CHARACTERS,
      &error_code, &error);
  if (value) {
    std::move(callback).Run(base::make_optional(std::move(*value)),
                            base::nullopt);
  } else {
    std::move(callback).Run(base::nullopt,
                            base::make_optional(std::move(error)));
  }
}

}  // namespace chrome_cleaner
