// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SegmentStream_h
#define SegmentStream_h

#include <algorithm>
#include "platform/PlatformExport.h"
#include "platform/wtf/RefPtr.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

class SegmentReader;

class PLATFORM_EXPORT SegmentStream : public SkStream {
 public:
  SegmentStream();
  SegmentStream(const SegmentStream&) = delete;
  SegmentStream& operator=(const SegmentStream&) = delete;
  SegmentStream(SegmentStream&&);
  SegmentStream& operator=(SegmentStream&&);

  ~SegmentStream() override;

  void SetReader(WTF::RefPtr<SegmentReader>);
  // If a buffer has shrunk beyond the point we have read, it has been cleared.
  // This allows clients to be aware of when data suddenly disappears.
  bool IsCleared() const;

  // From SkStream:
  size_t read(void* buffer, size_t) override;
  size_t peek(void* buffer, size_t) const override;
  bool isAtEnd() const override;
  bool rewind() override;
  bool hasPosition() const override { return true; }
  size_t getPosition() const override { return position_; }
  bool seek(size_t position) override;
  bool move(long offset) override;
  bool hasLength() const override { return true; }
  size_t getLength() const override;

 private:
  WTF::RefPtr<SegmentReader> reader_;
  size_t position_ = 0;
};

}  // namespace blink

#endif
