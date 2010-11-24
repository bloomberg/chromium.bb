// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_SIMPLE_BUFFER_H__
#define NET_TOOLS_FLIP_SERVER_SIMPLE_BUFFER_H__
#pragma once

#include <string>

#include "net/tools/flip_server/buffer_interface.h"

namespace net {

class SimpleBuffer : public BufferInterface {
 public:
  SimpleBuffer();
  explicit SimpleBuffer(int size);
  virtual ~SimpleBuffer() {
    delete[] storage_;
  }

  std::string str() const;

  typedef char * iterator;
  typedef const char * const_iterator;

  iterator begin() { return storage_ + read_idx_; }
  const_iterator begin() const { return storage_ + read_idx_; }

  iterator end() { return storage_ + write_idx_; }
  const_iterator end() const { return storage_ + write_idx_; }

  // The following functions all override pure virtual functions
  // in BufferInterface. See buffer_interface.h for a description
  // of what they do.
  virtual int ReadableBytes() const;
  virtual int BufferSize() const;
  virtual int BytesFree() const;

  virtual bool Empty() const;
  virtual bool Full() const;

  virtual int Write(const char* bytes, int size);

  virtual void GetWritablePtr(char **ptr, int* size) const;

  virtual void GetReadablePtr(char **ptr, int* size) const;

  virtual int Read(char* bytes, int size);

  virtual void Clear();

  // This can be an expensive operation: costing a new/delete, and copying of
  // all existing data. Even if the existing buffer does not need to be
  // resized, unread data may still need to be non-destructively copied to
  // consolidate fragmented free space.
  virtual bool Reserve(int size);

  virtual void AdvanceReadablePtr(int amount_to_advance);

  virtual void AdvanceWritablePtr(int amount_to_advance);

  void Swap(SimpleBuffer* other) {
    char* tmp = storage_;
    storage_ = other->storage_;
    other->storage_ = tmp;

    int tmp_int = write_idx_;
    write_idx_ = other->write_idx_;
    other->write_idx_ = tmp_int;

    tmp_int = read_idx_;
    read_idx_ = other->read_idx_;
    other->read_idx_ = tmp_int;

    tmp_int = storage_size_;
    storage_size_ = other->storage_size_;
    other->storage_size_ = tmp_int;
  }

 protected:
  char* storage_;
  int write_idx_;
  int read_idx_;
  int storage_size_;

 private:
  //DISALLOW_COPY_AND_ASSIGN(SimpleBuffer);
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_SIMPLE_BUFFER_H__

