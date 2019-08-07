// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_JSON_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_JSON_H_

#include <string>

#include "base/macros.h"
#include "components/update_client/protocol_parser.h"

namespace update_client {

// Parses responses for the update protocol version 3.
// (https://github.com/google/omaha/blob/wiki/ServerProtocolV3.md)
class ProtocolParserJSON final : public ProtocolParser {
 public:
  ProtocolParserJSON() = default;

 private:
  // Overrides for ProtocolParser.
  bool DoParse(const std::string& response_json, Results* results) override;

  DISALLOW_COPY_AND_ASSIGN(ProtocolParserJSON);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_JSON_H_
