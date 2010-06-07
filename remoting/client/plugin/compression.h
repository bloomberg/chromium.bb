// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_COMPRESSION_H_
#define REMOTING_CLIENT_PLUGIN_COMPRESSION_H_

#include <vector>

#include "base/basictypes.h"

#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif

namespace remoting {

class Compressor {
 public:
  Compressor() {}
  virtual ~Compressor() {}

  virtual void Write(char* buffer, int size) = 0;
  virtual void Flush() = 0;
  virtual int GetCompressedSize() = 0;
  virtual char* GetCompressedData() = 0;
  virtual int GetRawSize() = 0;

  DISALLOW_COPY_AND_ASSIGN(Compressor);
};

class Decompressor {
 public:
  Decompressor() {}
  virtual ~Decompressor() {}

  virtual void Write(char* buffer, int size) = 0;
  virtual void Flush() = 0;
  virtual char* GetRawData() = 0;
  virtual int GetRawSize() = 0;
  virtual int GetCompressedSize() = 0;

  DISALLOW_COPY_AND_ASSIGN(Decompressor);
};

class ZCompressor : public Compressor {
 public:
  ZCompressor();
  virtual ~ZCompressor() {}

  virtual void Write(char* buffer, int size);
  virtual void Flush();
  virtual int GetCompressedSize();
  virtual char* GetCompressedData();
  virtual int GetRawSize();

 private:
  void WriteInternal(char* buffer, int size, int flush);

  std::vector<char> buffer_;
  z_stream stream_;

  DISALLOW_COPY_AND_ASSIGN(ZCompressor);
};

class ZDecompressor : public Decompressor {
 public:
  ZDecompressor();
  virtual ~ZDecompressor() {}

  virtual void Write(char* buffer, int size);
  virtual void Flush();
  virtual char* GetRawData();
  virtual int GetRawSize();
  virtual int GetCompressedSize();

 private:
  void WriteInternal(char* buffer, int size, int flush);

  std::vector<char> buffer_;
  z_stream stream_;

  DISALLOW_COPY_AND_ASSIGN(ZDecompressor);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_COMPRESSION_H_
