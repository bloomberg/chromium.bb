// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_GRAPHICS_2D_H_
#define PPAPI_CPP_GRAPHICS_2D_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"

namespace pp {

class CompletionCallback;
class ImageData;
class Point;
class Rect;

class Graphics2D : public Resource {
 public:
  // Creates an is_null() ImageData object.
  Graphics2D();

  // The copied context will refer to the original (since this is just a wrapper
  // around a refcounted resource).
  Graphics2D(const Graphics2D& other);

  // Allocates a new 2D graphics context with the given size in the browser,
  // resulting object will be is_null() if the allocation failed.
  Graphics2D(const Size& size, bool is_always_opaque);

  virtual ~Graphics2D();

  Graphics2D& operator=(const Graphics2D& other);
  void swap(Graphics2D& other);

  const Size& size() const { return size_; }

  // Enqueues paint or scroll commands. THIS COMMAND HAS NO EFFECT UNTIL YOU
  // CALL Flush().
  //
  // If you call the version with no source rect, the entire image will be
  // painted.
  //
  // Please see PPB_Graphics2D.PaintImageData / .Scroll for more details.
  void PaintImageData(const ImageData& image,
                      const Point& top_left);
  void PaintImageData(const ImageData& image,
                      const Point& top_left,
                      const Rect& src_rect);
  void Scroll(const Rect& clip, const Point& amount);

  // The browser will take ownership of the given image data. The object
  // pointed to by the parameter will be cleared. To avoid horrible artifacts,
  // you should also not use any other ImageData objects referring to the same
  // resource will no longer be usable. THIS COMMAND HAS NO EFFECT UNTIL YOU
  // CALL Flush().
  //
  // Please see PPB_Graphics2D.ReplaceContents for more details.
  void ReplaceContents(ImageData* image);

  // Flushes all the currently enqueued Paint, Scroll, and Replace commands.
  // Can be used in synchronous mode (NULL callback pointer) from background
  // threads.
  //
  // Please see PPB_Graphics2D.Flush for more details.
  int32_t Flush(const CompletionCallback& cc);

 private:
  Size size_;
};

}  // namespace pp

#endif  // PPAPI_CPP_GRAPHICS_2D_H_
