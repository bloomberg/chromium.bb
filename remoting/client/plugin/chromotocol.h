// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTOCOL_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTOCOL_H_

#include "base/scoped_ptr.h"

namespace remoting {

class HostConnection;

enum ControlMessage {
  MessageInit,
  MessageUpdate,
  MessageMouse,
};

struct InitMessage {
  int message;
  int compression;
  int width;
  int height;
};

struct MouseMessage {
  int message;
  int x, y;
  int flags;
};

enum MouseFlag {
  LeftDown = 1 << 1,
  LeftUp = 1 << 2,
  RightDown = 1 << 3,
  RightUp = 1 << 4
};

struct UpdateMessage {
  int message;
  int num_diffs;
  int compression;
  int compressed_size;
};

enum ImageFormat {
  FormatRaw,
  FormatJpeg,  // Not used
  FormatPng,   // Not used
  FormatZlib,  // Not used
  FormatVp8,
};

enum Compression {
  CompressionNone,
  CompressionZlib,
};

struct BinaryImageHeader {
  BinaryImageHeader()
      : format(FormatRaw), x(0), y(0), width(0), height(0), size(0) {}

  ImageFormat format;
  int x;
  int y;
  int width;
  int height;
  int size;
};

struct BinaryImage {
  BinaryImageHeader header;
  scoped_array<char> data;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTOCOL_H_
