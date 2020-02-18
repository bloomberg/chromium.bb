// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_JSON_PARSER_API_H_
#define CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_JSON_PARSER_API_H_

#include "base/callback.h"
#include "base/optional.h"
#include "base/values.h"

namespace chrome_cleaner {

using ParseDoneCallback =
    base::OnceCallback<void(base::Optional<base::Value>,
                            const base::Optional<std::string>&)>;

class JsonParserAPI {
 public:
  virtual ~JsonParserAPI() = default;

  virtual void Parse(const std::string& json, ParseDoneCallback callback) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_JSON_PARSER_API_H_
