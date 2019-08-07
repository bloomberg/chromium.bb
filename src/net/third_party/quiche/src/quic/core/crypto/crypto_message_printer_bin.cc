// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dumps the contents of a QUIC crypto handshake message in a human readable
// format.
//
// Usage: crypto_message_printer_bin <hex of message>

#include <iostream>
#include <string>

#include "base/init_google.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_framer.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_text_utils.h"

using quic::Perspective;
using std::cerr;
using std::cout;
using std::endl;

namespace quic {

class CryptoMessagePrinter : public ::quic::CryptoFramerVisitorInterface {
 public:
  void OnHandshakeMessage(const CryptoHandshakeMessage& message) override {
    cout << message.DebugString() << endl;
  }

  void OnError(CryptoFramer* framer) override {
    cerr << "Error code: " << framer->error() << endl;
    cerr << "Error details: " << framer->error_detail() << endl;
  }
};

}  // namespace quic

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);

  quic::CryptoMessagePrinter printer;
  quic::CryptoFramer framer;
  framer.set_visitor(&printer);
  framer.set_process_truncated_messages(true);
  std::string input = quic::QuicTextUtils::HexDecode(argv[1]);
  if (!framer.ProcessInput(input)) {
    return 1;
  }
  if (framer.InputBytesRemaining() != 0) {
    cerr << "Input partially consumed. " << framer.InputBytesRemaining()
         << " bytes remaining." << endl;
    return 2;
  }
  return 0;
}
