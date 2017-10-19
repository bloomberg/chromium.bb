/*
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#ifndef Path2D_h
#define Path2D_h

#include "core/svg/SVGMatrixTearOff.h"
#include "core/svg/SVGPathUtilities.h"
#include "modules/canvas2d/CanvasPath.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

class MODULES_EXPORT Path2D final : public GarbageCollectedFinalized<Path2D>,
                                    public CanvasPath,
                                    public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(Path2D);

 public:
  static Path2D* Create() { return new Path2D; }
  static Path2D* Create(const String& path_data) {
    return new Path2D(path_data);
  }
  static Path2D* Create(Path2D* path) { return new Path2D(path); }
  static Path2D* Create(const Path& path) { return new Path2D(path); }

  const Path& GetPath() const { return path_; }

  void addPath(Path2D* path) { addPath(path, 0); }

  void addPath(Path2D* path, SVGMatrixTearOff* transform) {
    Path src = path->GetPath();
    path_.AddPath(src, transform ? transform->Value()
                                 : AffineTransform(1, 0, 0, 1, 0, 0));
  }

  ~Path2D() override {}
  void Trace(blink::Visitor* visitor) {}

 private:
  Path2D() : CanvasPath() {}

  Path2D(const Path& path) : CanvasPath(path) {}

  Path2D(Path2D* path) : CanvasPath(path->GetPath()) {}

  Path2D(const String& path_data) : CanvasPath() {
    BuildPathFromString(path_data, path_);
  }
};

}  // namespace blink

#endif  // Path2D_h
