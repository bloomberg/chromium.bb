#include "rsp_packetizer.h"
#include "debug_blob.h"

// Description of GDB RSP protocol:
// http://sources.redhat.com/gdb/current/onlinedocs/gdb.html#Remote-Protocol

namespace rsp {

Packetizer::Packetizer()
  : state_(IDLE),
    consumer_(NULL),
    calculated_checksum_(0),
    recv_checksum_(0) {
  Reset();
}
 
Packetizer::~Packetizer() {
}

void Packetizer::SetPacketConsumer(PacketConsumer* consumer) {
  consumer_ = consumer;
}

void Packetizer::Reset() {
  state_ = IDLE;
  body_.Clear();
  calculated_checksum_ = 0;
  recv_checksum_ = 0;
}

bool Packetizer::IsIdle() const {
  return (state_ == IDLE);
}

void Packetizer::OnData(const debug::Blob& data) {
  for (size_t i = 0; i < data.Size(); i++)
    OnChar(data[i]);
}

void Packetizer::OnData(const void* data, size_t data_length) {
  const unsigned char* cdata = static_cast<const unsigned char*>(data);
  if (NULL == data)
    return;
  if (-1 == data_length)
    data_length = static_cast<int>(strlen(static_cast<const char*>(data)));
  if (data_length < 0)
    return;
  for (size_t i = 0; i < data_length; i++) {
    unsigned char c = *cdata++;
    OnChar(c);
  }
}

void Packetizer::OnChar(unsigned char c) {
  if (NULL == consumer_)
    return;

  if (((BODY == state_) && ('#' != c)) ||
     (ESCAPE == state_) ||
     (RUNLEN == state_)) 
      AddToChecksum(c);

  switch (state_) {
    case IDLE: {
      if (('+' == c) || ('-' == c))
        return;
      if ('$' == c)
        state_ = BODY;
      else if (3 == c)
        consumer_->OnBreak();
      else
        consumer_->OnUnexpectedChar(c);
      break;      
    }
    case BODY: {
      if ('}' == c)
        state_ = ESCAPE;
      else if ('#' == c)
        state_ = END;
      else if ('*' == c)
        state_ = RUNLEN;
      else if (c > 126)
        consumer_->OnUnexpectedChar(c);
      else
        AddCharToBody(c);
      break;
    }
    case ESCAPE: {
      c = c ^ 0x20;
      AddCharToBody(c);
      state_ = BODY;
      break;
    }        
    case RUNLEN: {
      int n = c - 29;
      AddRepeatedChars(n);
      state_ = BODY;
      break;
    }
    case END: {
      if (debug::Blob::HexCharToInt(c, &recv_checksum_)) {
        recv_checksum_ <<= 4;
        state_ = CHECKSUM;
      }
      break;
    }
    case CHECKSUM: {
      unsigned int digit = 0;
      if (debug::Blob::HexCharToInt(c, &digit)) {
        recv_checksum_ += digit;
        bool checksum_valid = (recv_checksum_ == calculated_checksum_);
        consumer_->OnPacket(body_, checksum_valid);
        Reset();
      }
      break;
    }
  }
}

void Packetizer::AddToChecksum(unsigned char c) {
  calculated_checksum_ += c;
  calculated_checksum_ %= 256;
}

void Packetizer::AddCharToBody(unsigned char c) {
  body_.PushBack(c);
}

void Packetizer::AddRepeatedChars(size_t n) {
  size_t len = body_.Size();
  if (0 != len) {
    char c = body_[len - 1];
    for (size_t i = 0; i < n; i++)
      AddCharToBody(c);
  }
}
}  // namespace rsp