// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CAPTURER_SHARED_BUFFER_FACTORY_H_
#define REMOTING_CAPTURER_SHARED_BUFFER_FACTORY_H_

namespace remoting {

class SharedBuffer;

// Provides a way to create shared buffers accessible by two or more processes.
class SharedBufferFactory {
 public:
  virtual ~SharedBufferFactory() {}

  // Creates a shared memory buffer of the given size.
  virtual scoped_refptr<SharedBuffer> CreateSharedBuffer(uint32 size) = 0;

  // Notifies the factory that the buffer is no longer used by the caller and
  // can be released. The caller still has to drop all references to free
  // the memory.
  virtual void ReleaseSharedBuffer(scoped_refptr<SharedBuffer> buffer) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CAPTURER_SHARED_BUFFER_FACTORY_H_
