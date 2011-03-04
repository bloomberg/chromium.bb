// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a simple X11 chromoting client.

#include <iostream>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_config.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_util.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/client/x11_view.h"
#include "remoting/client/x11_input_handler.h"
#include "remoting/protocol/connection_to_host.h"

void ClientQuit(MessageLoop* loop) {
  loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;

  remoting::ClientConfig config;
  if (!remoting::GetLoginInfoFromArgs(argc, argv, &config)) {
    std::cout << "Unable to obtain login info" << std::endl;
    return 1;
  }

  MessageLoop ui_loop;
  remoting::ClientContext context;
  remoting::protocol::ConnectionToHost connection(context.jingle_thread(),
                                                  NULL, NULL);
  remoting::X11View view;
  scoped_refptr<remoting::RectangleUpdateDecoder> rectangle_decoder =
      new remoting::RectangleUpdateDecoder(context.decode_message_loop(),
                                           &view);
  remoting::X11InputHandler input_handler(&context, &connection, &view);
  remoting::ChromotingClient client(
      config, &context, &connection, &view, rectangle_decoder, &input_handler,
      NewRunnableFunction(&ClientQuit, &ui_loop));

  // Run the client on a new MessageLoop until
  context.Start();
  client.Start();
  ui_loop.Run();

  client.Stop();
  context.Stop();

  return 0;
}
