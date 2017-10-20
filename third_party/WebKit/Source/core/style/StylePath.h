// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePath_h
#define StylePath_h

#include <memory>
#include "core/style/BasicShapes.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class CSSValue;
class Path;
class SVGPathByteStream;

class StylePath final : public BasicShape {
 public:
  static scoped_refptr<StylePath> Create(std::unique_ptr<SVGPathByteStream>);
  ~StylePath();

  static StylePath* EmptyPath();

  const Path& GetPath() const;
  float length() const;
  bool IsClosed() const;

  const SVGPathByteStream& ByteStream() const { return *byte_stream_; }

  CSSValue* ComputedCSSValue() const;

  void GetPath(Path&, const FloatRect&) override;
  scoped_refptr<BasicShape> Blend(const BasicShape*, double) const override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kStylePathType; }

 private:
  explicit StylePath(std::unique_ptr<SVGPathByteStream>);

  std::unique_ptr<SVGPathByteStream> byte_stream_;
  mutable std::unique_ptr<Path> path_;
  mutable float path_length_;
};

DEFINE_BASICSHAPE_TYPE_CASTS(StylePath);

}  // namespace blink

#endif  // StylePath_h
