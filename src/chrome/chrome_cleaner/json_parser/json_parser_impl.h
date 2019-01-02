// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_JSON_PARSER_JSON_PARSER_IMPL_H_
#define CHROME_CHROME_CLEANER_JSON_PARSER_JSON_PARSER_IMPL_H_

#include "chrome/chrome_cleaner/interfaces/json_parser.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace chrome_cleaner {

class JsonParserImpl : public mojom::JsonParser {
 public:
  explicit JsonParserImpl(mojom::JsonParserRequest request,
                          base::OnceClosure connection_error_handler);
  ~JsonParserImpl() override;

  // mojom::JsonParser
  void Parse(const std::string& json, ParseCallback callback) override;

 private:
  mojo::Binding<mojom::JsonParser> binding_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_JSON_PARSER_JSON_PARSER_IMPL_H_
