#pragma once
#include "my_json.h"
#include "..\common\debug_blob.h"

namespace rsp {
class Parser {
 public:
  Parser();

  bool FromRspRequestToJson(debug::Blob& msg, json::Object* out);
  bool FromJsonToRspReply(json::Object& msg, debug::Blob* out);

 protected:
  std::string last_request_;
};

};  // namespace rsp
