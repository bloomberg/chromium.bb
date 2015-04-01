// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_
#define NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_extension.h"

namespace net {

class NET_EXPORT_PRIVATE WebSocketExtensionParser {
 public:
  WebSocketExtensionParser();
  ~WebSocketExtensionParser();

  // Parses the given string as a Sec-WebSocket-Extensions header value.
  //
  // There must be no newline characters in the input. LWS-concatenation must
  // have already been done before calling this method.
  void Parse(const char* data, size_t size);
  void Parse(const std::string& data) {
    Parse(data.data(), data.size());
  }

  // Returns true if the last Parse() method call encountered any syntax error.
  bool has_error() const { return has_error_; }
  // Returns the result of the last Parse() method call.
  const std::vector<WebSocketExtension>& extensions() const {
    return extensions_;
  }

 private:
  // TODO(tyoshino): Make the following methods return success/fail.
  void Consume(char c);
  void ConsumeExtension(WebSocketExtension* extension);
  void ConsumeExtensionParameter(WebSocketExtension::Parameter* parameter);
  void ConsumeToken(base::StringPiece* token);
  void ConsumeQuotedToken(std::string* token);
  void ConsumeSpaces();
  bool Lookahead(char c);
  bool ConsumeIfMatch(char c);
  size_t UnconsumedBytes() const { return end_ - current_; }

  static bool IsControl(char c);
  static bool IsSeparator(char c);

  // The current position in the input string.
  const char* current_;
  // The pointer of the end of the input string.
  const char* end_;
  bool has_error_;
  std::vector<WebSocketExtension> extensions_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketExtensionParser);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_
