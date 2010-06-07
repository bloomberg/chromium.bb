// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_CLIENT_H_
#define REMOTING_CLIENT_PLUGIN_CLIENT_H_

#include <string>

#include "base/scoped_ptr.h"
#include "remoting/client/plugin/chromoting_plugin.h"

namespace remoting {

class BinaryImage;
class BinaryImageHeader;
class HostConnection;

class ChromotingClient {
 public:
  explicit ChromotingClient(ChromotingPlugin* plugin);
  virtual ~ChromotingClient();

  void hexdump(void *ptr, int buflen);

  void merge_image(BinaryImageHeader* header, char* data);
  void draw(int width, int height, NPDeviceContext2D* context);

  bool connect_to_host(const std::string ip);
  void print_host_ip_prompt();

  void set_window();

  void handle_char_event(NPPepperEvent* npevent);
  void handle_login_char(char ch);

  void handle_mouse_event(NPPepperEvent* npevent);
  void send_mouse_message(NPPepperEvent* event);

  bool handle_update_message();

  bool check_image_header(BinaryImageHeader* header);
  bool read_image(BinaryImage* image);

 private:
  // Pepper plugin (communicate with browser).
  ChromotingPlugin* plugin_;

  // Network connection (communicate with remote host machine).
  HostConnection* host_;

  // IP address of remote host machine.
  std::string host_ip_address_;

  // Screen bitmap image.
  scoped_ptr<BinaryImage> screen_;

  // Display extended output messages (for debugging).
  bool verbose_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingClient);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CLIENT_H_
