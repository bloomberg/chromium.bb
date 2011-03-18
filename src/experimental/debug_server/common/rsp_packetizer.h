#ifndef NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_RSP_TOKENIZER_H_
#define NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_RSP_TOKENIZER_H_

#include <deque>
#include <string>
#include "debug_blob.h"
#include "rsp_packet_util.h"

namespace rsp {
class PacketConsumer {
 public:
  PacketConsumer() {}
  virtual ~PacketConsumer() {}
  virtual void OnPacket(debug::Blob& body, bool valid_checksum) = 0;
  virtual void OnUnexpectedChar(char unexpected_char) = 0;
  virtual void OnBreak() = 0;
};

class Packetizer {
public:
  Packetizer();
  virtual ~Packetizer();

  virtual void SetPacketConsumer(PacketConsumer* consumer);
  virtual void OnData(const void* data, size_t data_length=-1);
  virtual void OnData(const debug::Blob& data);
  virtual void Reset();
  virtual bool IsIdle() const;

protected:
  enum State {IDLE, BODY, END, CHECKSUM, ESCAPE, RUNLEN};

  virtual void OnChar(unsigned char c);
  virtual void AddToChecksum(unsigned char c);
  virtual void AddCharToBody(unsigned char c);
  virtual void AddRepeatedChars(size_t n);

  State state_;
  PacketConsumer* consumer_;
  debug::Blob body_;
  unsigned int calculated_checksum_;
  unsigned int recv_checksum_;
};

}  // namespace rsp

#endif  // NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_RSP_TOKENIZER_H_