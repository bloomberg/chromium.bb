#include "rsp_packet_util.h"
#include "rsp_packetizer.h"

// Description of GDB RSP protocol:
// http://sources.redhat.com/gdb/current/onlinedocs/gdb.html#Remote-Protocol

namespace {
class LocalPacketConsumer : public rsp::PacketConsumer {
 public:
  LocalPacketConsumer(debug::Blob* packet)
      : packet_(packet), success_(false) {}
  virtual void OnPacket(debug::Blob& body, bool valid_checksum) {
    *packet_ = body;
    success_ = true;
  }
  virtual void OnUnexpectedChar(char unexpected_char) {}
  virtual void OnBreak() {}

  debug::Blob* packet_;
  bool success_;
};

void Escape(const debug::Blob& blob_in, debug::Blob* blob_out);
char GetHexDigit(unsigned int value, int digit_position);
}  // namespace

namespace rsp {
void PacketUtils::AddEnvelope(const debug::Blob& blob_in,
                              debug::Blob* blob_out) {
  blob_out->Clear();
  Escape(blob_in, blob_out);
  unsigned long checksum = 0;
  for (size_t i = 0; i < blob_out->Size(); i++) {
    checksum += (*blob_out)[i];
    checksum %= 256;
  }

  blob_out->PushFront('$');
  blob_out->PushBack('#');
  blob_out->PushBack(GetHexDigit(checksum, 1));
  blob_out->PushBack(GetHexDigit(checksum, 0));
}

bool PacketUtils::RemoveEnvelope(const debug::Blob& blob_in,
                                 debug::Blob* blob_out) {
  blob_out->Clear();
  Packetizer pktz;
  LocalPacketConsumer consumer(blob_out);
  pktz.SetPacketConsumer(&consumer);
  pktz.OnData(blob_in);
  return consumer.success_;
}
}  // namespace rsp

namespace {
void Escape(const debug::Blob& blob_in, debug::Blob* blob_out) {
  blob_out->Clear();
  unsigned char prev_c = 0;
  for (size_t i = 0; i < blob_in.Size(); i++) {
    unsigned char c = blob_in[i];
    if (((('$' == c) || ('#' == c) || ('*' == c) || ('}' == c) || (3 == c)) &&
        (prev_c != '*')) || (c > 126)) {
      // escape it by '}'
      blob_out->PushBack('}');
      c = c ^ 0x20;
    }
    blob_out->PushBack(c);
    prev_c = blob_in[i];
  }
}

char GetHexDigit(unsigned int value, int digit_position) {
  const char* kDigitStrings = "0123456789ABCDEF";
  unsigned int digit = (value >> (4 * digit_position)) & 0xF;
  return kDigitStrings[digit];
}
}  // namespace

