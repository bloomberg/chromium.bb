// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePath_h
#define StylePath_h

#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class CSSValue;
class Path;
class SVGPathByteStream;

class StylePath : public RefCounted<StylePath> {
 public:
  static PassRefPtr<StylePath> Create(std::unique_ptr<SVGPathByteStream>);
  ~StylePath();

  static StylePath* EmptyPath();

  const Path& GetPath() const;
  float length() const;
  bool IsClosed() const;

  const SVGPathByteStream& ByteStream() const { return *byte_stream_; }

  CSSValue* ComputedCSSValue() const;

  bool operator==(const StylePath&) const;

 private:
  explicit StylePath(std::unique_ptr<SVGPathByteStream>);

  std::unique_ptr<SVGPathByteStream> byte_stream_;
  mutable std::unique_ptr<Path> path_;
  mutable float path_length_;
};

}  // namespace blink

#endif  // StylePath_h
