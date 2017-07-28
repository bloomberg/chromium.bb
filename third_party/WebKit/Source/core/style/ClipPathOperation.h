/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ClipPathOperation_h
#define ClipPathOperation_h

#include <memory>
#include "core/style/BasicShapes.h"
#include "core/svg/SVGElementProxy.h"
#include "platform/graphics/Path.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class SVGElement;
class SVGResourceClient;
class TreeScope;

class ClipPathOperation : public RefCounted<ClipPathOperation> {
 public:
  enum OperationType { REFERENCE, SHAPE };

  virtual ~ClipPathOperation() {}

  virtual bool operator==(const ClipPathOperation&) const = 0;
  bool operator!=(const ClipPathOperation& o) const { return !(*this == o); }

  virtual OperationType GetType() const = 0;
  bool IsSameType(const ClipPathOperation& o) const {
    return o.GetType() == GetType();
  }

 protected:
  ClipPathOperation() {}
};

class ReferenceClipPathOperation final : public ClipPathOperation {
 public:
  static RefPtr<ReferenceClipPathOperation> Create(
      const String& url,
      SVGElementProxy& element_proxy) {
    return AdoptRef(new ReferenceClipPathOperation(url, element_proxy));
  }

  void AddClient(SVGResourceClient*);
  void RemoveClient(SVGResourceClient*);

  SVGElement* FindElement(TreeScope&) const;

  const String& Url() const { return url_; }

 private:
  bool operator==(const ClipPathOperation&) const override;
  OperationType GetType() const override { return REFERENCE; }

  ReferenceClipPathOperation(const String& url, SVGElementProxy& element_proxy)
      : element_proxy_(&element_proxy), url_(url) {}

  Persistent<SVGElementProxy> element_proxy_;
  String url_;
};

DEFINE_TYPE_CASTS(ReferenceClipPathOperation,
                  ClipPathOperation,
                  op,
                  op->GetType() == ClipPathOperation::REFERENCE,
                  op.GetType() == ClipPathOperation::REFERENCE);

class ShapeClipPathOperation final : public ClipPathOperation {
 public:
  static RefPtr<ShapeClipPathOperation> Create(RefPtr<BasicShape> shape) {
    return AdoptRef(new ShapeClipPathOperation(std::move(shape)));
  }

  const BasicShape* GetBasicShape() const { return shape_.Get(); }
  bool IsValid() const { return shape_.Get(); }
  const Path& GetPath(const FloatRect& bounding_rect) {
    DCHECK(shape_);
    path_.reset();
    path_ = WTF::WrapUnique(new Path);
    shape_->GetPath(*path_, bounding_rect);
    path_->SetWindRule(shape_->GetWindRule());
    return *path_;
  }

 private:
  bool operator==(const ClipPathOperation&) const override;
  OperationType GetType() const override { return SHAPE; }

  ShapeClipPathOperation(RefPtr<BasicShape> shape) : shape_(std::move(shape)) {}

  RefPtr<BasicShape> shape_;
  std::unique_ptr<Path> path_;
};

DEFINE_TYPE_CASTS(ShapeClipPathOperation,
                  ClipPathOperation,
                  op,
                  op->GetType() == ClipPathOperation::SHAPE,
                  op.GetType() == ClipPathOperation::SHAPE);

inline bool ShapeClipPathOperation::operator==(
    const ClipPathOperation& o) const {
  if (!IsSameType(o))
    return false;
  BasicShape* other_shape = ToShapeClipPathOperation(o).shape_.Get();
  if (!shape_.Get() || !other_shape)
    return static_cast<bool>(shape_.Get()) == static_cast<bool>(other_shape);
  return *shape_ == *other_shape;
}

}  // namespace blink

#endif  // ClipPathOperation_h
