// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_GRAPHICS_2D_API_H_
#define PPAPI_THUNK_PPB_GRAPHICS_2D_API_H_

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Graphics2D_API {
 public:
  virtual ~PPB_Graphics2D_API() {}

  virtual PP_Bool Describe(PP_Size* size, PP_Bool* is_always_opaque) = 0;
  virtual void PaintImageData(PP_Resource image_data,
                              const PP_Point* top_left,
                              const PP_Rect* src_rect) = 0;
  virtual void Scroll(const PP_Rect* clip_rect,
                      const PP_Point* amount) = 0;
  virtual void ReplaceContents(PP_Resource image_data) = 0;
  virtual bool SetScale(float scale) = 0;
  virtual float GetScale() = 0;

  // When |old_image_data| is non-null and the flush is executing a replace
  // contents (which leaves the "old" ImageData unowned), the resource ID of
  // the old image data will be placed into |*old_image_data| synchronously
  // (not when the flush callback completes).
  //
  // When this happens, a reference to this resource will be transferred to the
  // caller. If there is no replace contents operation, old_image_data will be
  // ignored. If |*old_image_data| is null, then the old image data will be
  // destroyed if there was one.
  virtual int32_t Flush(scoped_refptr<TrackedCallback> callback,
                        PP_Resource* old_image_data) = 0;

  // Test only
  virtual bool ReadImageData(PP_Resource image, const PP_Point* top_left) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_GRAPHICS_2D_API_H_
