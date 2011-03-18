#ifndef NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_UNIT_TESTS_RSP_TOKENIZER_TEST_H_
#define NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_UNIT_TESTS_RSP_TOKENIZER_TEST_H_

#pragma once
#include <string>
#include "..\common\rsp_packetizer.h"

namespace rsp {
class Packetizer_test : public PacketConsumer {
public:
  Packetizer_test();
  ~Packetizer_test();

  bool Run(std::string* error);

protected:
  void Clean();
  bool CompareTokenBody(const char* text);
  virtual void OnPacket(debug::Blob& body, bool valid_checksum);
  virtual void OnUnexpectedChar(char unexpected_char);
  virtual void OnBreak();

  enum ResponseType {NONE, PACKET, INVALID_CHAR, BREAK};
  ResponseType response_type_;
  int response_num_;  // should be 1 for passed tests.
  debug::Blob packet_body_;
  bool token_is_checksum_valid_;
};
}  //namespace rsp

#endif  // NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_UNIT_TESTS_RSP_TOKENIZER_TEST_H_