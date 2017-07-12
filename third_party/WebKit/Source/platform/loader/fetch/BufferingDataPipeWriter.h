// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BufferingDataPipeWriter_h
#define BufferingDataPipeWriter_h

#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Vector.h"

namespace blink {

// A writer to a mojo data pipe which has a buffer to store contents. As a
// result, it is possible for a caller to miss write failures.
class PLATFORM_EXPORT BufferingDataPipeWriter {
 public:
  BufferingDataPipeWriter(mojo::ScopedDataPipeProducerHandle, WebTaskRunner*);

  // Writes buffer[0:num_bytes] to the data pipe. Returns true if there is no
  // error.
  bool Write(const char* buffer, uint32_t num_bytes);

  // Finishes writing. After calling this function, Write must not be called.
  void Finish();

 private:
  void OnWritable(MojoResult, const mojo::HandleSignalsState&);
  void ClearIfNeeded();
  void Clear();

  mojo::ScopedDataPipeProducerHandle handle_;
  mojo::SimpleWatcher watcher_;
  Deque<Vector<char>> buffer_;
  size_t front_written_size_ = 0;
  bool waiting_ = false;
  bool finished_ = false;
};

}  // namespace blink

#endif
