// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <UIKit/UIKit.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "media/base/yuv_convert.h"
#include "net/socket/ssl_server_socket.h"

#import "app_delegate.h"

int main(int argc, char* argv[]) {
  // This class is designed to fulfill the dependents needs when it goes out of
  // scope and gets destructed
  base::AtExitManager exitManager;

  // Publicize the CommandLine
  CommandLine::Init(argc, argv);

#ifdef DEBUG
  // Set min log level for debug builds.  For some reason this has to be
  // negative.
  logging::SetMinLogLevel(-1);
#endif

  // Allows later decoding of video frames.
  media::InitializeCPUSpecificYUVConversions();

  // Enable support for SSL server sockets, which must be done as early as
  // possible, preferably before any NSS SSL sockets (client or server) have
  // been created.
  net::EnableSSLServerSockets();

  @autoreleasepool {
    return UIApplicationMain(
        argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}
