// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/json_parser_impl.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"

namespace data_decoder {

JsonParserImpl::JsonParserImpl() = default;

JsonParserImpl::~JsonParserImpl() = default;

void JsonParserImpl::Parse(const std::string& json,
                           uint32_t options,
                           ParseCallback callback) {
  base::JSONReader::ValueWithError ret =
      base::JSONReader::ReadAndReturnValueWithError(json, options);
  if (ret.value) {
    std::move(callback).Run(std::move(ret.value), absl::nullopt);
  } else {
    std::move(callback).Run(absl::nullopt,
                            absl::make_optional(std::move(ret.error_message)));
  }
}

}  // namespace data_decoder
