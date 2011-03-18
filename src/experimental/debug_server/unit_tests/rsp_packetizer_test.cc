#include "rsp_packetizer_test.h"
#include <stdio.h>
#include "..\common\rsp_packetizer.h"

namespace {
unsigned long test_table[] = {
    0x34969443,     0xD76AA478,    0xE8C7B756,    0x242070DB,    0xC1BDCEEE,    
	  0xF57C0FAF,     0x4787C62A,    0xA8304613,    0xFD469501,    0x698098D8,
    0x8B44F7AF,     0xFFFF5BB1,    0x895CD7BE,    0x6B901122,    0xFD987193,
    0xA679438E,     0x49B40821,    0xF61E2562,    0xC040B340,    0x265E5A51,
    0xE9B6C7AA,     0xD62F105D,    0x02441453,    0xD8A1E681,    0xE7D3FBC8,
    0x21E1CDE6,     0xC33707D6,    0xF4D50D87,    0x455A14ED,    0xA9E3E905,
    0xFCEFA3F8,     0x676F02D9,    0x8D2A4C8A,    0xFFFA3942,    0x8771F681,
    0x6D9D6122,     0xFDE5380C,    0xA4BEEA44,    0x4BDECFA9,    0xF6BB4B60,
    0xBEBFBC70,     0x289B7EC6,    0xEAA127FA,    0xD4EF3085,    0x04881D05,
    0xD9D4D039,     0xE6DB99E5,    0x1FA27CF8,    0xC4AC5665,    0xF4292244,
    0x432AFF97,     0xAB9423A7,    0xFC93A039,    0x655B59C3,    0x8F0CCC92,
    0xFFEFF47D,     0x85845DD1,    0x6FA87E4F,    0xFE2CE6E0,    0xA3014314,
    0x4E0811A1,     0xF7537E82,    0xBD3AF235,    0x2AD7D2BB,    0xEB86D391,
    0x34969443,     0xD76AA478,    0xE8C7B756,    0x242070DB,    0xC1BDCEEE,    
	  0xF57C0FAF,     0x4787C62A,    0xA8304613,    0xFD469501,    0x698098D8,
    0x8B44F7AF,     0xFFFF5BB1,    0x895CD7BE,    0x6B901122,    0xFD987193,
    0xA679438E,     0x49B40821,    0xF61E2562,    0xC040B340,    0x265E5A51,
    0xE9B6C7AA,     0xD62F105D,    0x02441453,    0xD8A1E681,    0xE7D3FBC8,
    0x21E1CDE6,     0xC33707D6,    0xF4D50D87,    0x455A14ED,    0xA9E3E905,
    0xFCEFA3F8,     0x676F02D9,    0x8D2A4C8A,    0xFFFA3942,    0x8771F681,
    0x6D9D6122,     0xFDE5380C,    0xA4BEEA44,    0x4BDECFA9,    0xF6BB4B60,
    0xBEBFBC70,     0x289B7EC6,    0xEAA127FA,    0xD4EF3085,    0x04881D05,
    0xD9D4D039,     0xE6DB99E5,    0x1FA27CF8,    0xC4AC5665,    0xF4292244,
    0x432AFF97,     0xAB9423A7,    0xFC93A039,    0x655B59C3,    0x8F0CCC92,
    0xFFEFF47D,     0x85845DD1,    0x6FA87E4F,    0xFE2CE6E0,    0xA3014314,
    0x4E0811A1,     0xF7537E82,    0xBD3AF235,    0x2AD7D2BB,    0xEB86D391  
};
}  // namespace

namespace rsp {

Packetizer_test::Packetizer_test(){
}

Packetizer_test::~Packetizer_test(){
}

void Packetizer_test::Clean() {
  response_type_ = NONE;
  response_num_ = 0;
  packet_body_.Clear();
  token_is_checksum_valid_ = true;
}

void Packetizer_test::OnPacket(debug::Blob& body, bool valid_checksum) {
  response_type_ = PACKET;
  response_num_++;
  packet_body_ = body;
  token_is_checksum_valid_ = valid_checksum;
}

void Packetizer_test::OnUnexpectedChar(char unexpected_char) {
  packet_body_.PushBack(unexpected_char);
  response_type_ = INVALID_CHAR;
  response_num_++;
}

void Packetizer_test::OnBreak() {
  response_type_ = BREAK;
  response_num_++;
}

bool Packetizer_test::CompareTokenBody(const char* text) {
  size_t str_len = strlen(text);
  size_t body_len = packet_body_.Size();
  if (str_len != body_len)
    return false;
  for (size_t i = 0; i < str_len; i++) {
    if (packet_body_[i] != text[i])
      return false;
  }
  return true;
}

#define ReportError(n) {\
  char message[300];\
  _snprintf_s(message, sizeof(message) - 1, "Error %d in rsp::Packetizer unit test #%d. File=[%s] Line=%d.", n, test_no, __FILE__, __LINE__);\
  if (NULL != error) *error = message;\
  return false;\
} while(0)

#define ExpectedResult(type, text, valid) \
  if (1 != response_num_) {\
    ReportError(1);\
  }\
  else if (response_type_ != type) {\
    ReportError(2);\
  }\
  else if (!CompareTokenBody(text)) {\
    ReportError(3);\
  }\
  else if (token_is_checksum_valid_ != valid) {\
    ReportError(4);\
  } test_no++

bool Packetizer_test::Run(std::string* error) {
  Packetizer pktz;
  pktz.SetPacketConsumer(this);
  int test_no = 1;

  Clean();
  pktz.OnData("$#00");
  ExpectedResult(PACKET, "", true);

  Clean();
  pktz.OnData("$#02");
  ExpectedResult(PACKET, "", false);

  Clean();
  pktz.OnData("$#20");
  ExpectedResult(PACKET, "", false);

  Clean();
  pktz.OnData("$  #40");
  ExpectedResult(PACKET, "  ", true);

  Clean();
  pktz.OnData("$123456#35");
  ExpectedResult(PACKET, "123456", true);

  Clean();
  pktz.OnData("$}]aa#9C");
  ExpectedResult(PACKET, "}aa", true);

  Clean();
  pktz.OnData("$0*##7D");
  ExpectedResult(PACKET, "0000000", true);

  Clean();
  pktz.OnData("#");
  ExpectedResult(INVALID_CHAR, "#", true);

  Clean();
  pktz.OnData("\x03");
  ExpectedResult(BREAK, "", true);

  size_t tab_sz = sizeof(test_table);
  unsigned char* p_tab = reinterpret_cast<unsigned char*>(&test_table);
  for (size_t i = 0; i < tab_sz; i++) {
    size_t len = p_tab[i++] % 50;
    if ((i + len) > tab_sz )
      len = tab_sz - i;

    unsigned char* p_data = &p_tab[i];
    debug::Blob msg;
    for (size_t k = 0; k < len; k++)
      msg.PushBack(p_data[i++]);

    debug::Blob msg_in;
    PacketUtils::AddEnvelope(msg, &msg_in);

    Clean();
    size_t data_len = msg_in.Size();
    char* data = (char*)malloc(data_len);
    for(size_t k = 0; k < data_len; k++)
      data[k] = msg_in[k];

    pktz.OnData(data, data_len);

    if (1 != response_num_) {
      ReportError(5);
    }
    else if (response_type_ != PACKET) {
      ReportError(6);
    }
    else if (!(packet_body_ == msg)) {
      ReportError(7);
    }
    else if (!token_is_checksum_valid_) {
      ReportError(8);
    }
    free(data);
    test_no++;
  }

  return true;
}
}  //namespace rsp
