// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_GRAPHICS_3D_H_
#define PPAPI_CPP_GRAPHICS_3D_H_

#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class Instance;
class Var;

class Graphics3D : public Resource {
 public:
  /// Creates an is_null() Graphics3D object.
  Graphics3D();

  /// Creates and initializes a 3D rendering context. The returned context is
  /// off-screen to start with. It must be attached to a plugin instance using
  /// Instance::BindGraphics to draw on the web page.
  ///
  /// @param[in] instance The instance that will own the new Graphics3D.
  ///
  /// @param[in] attrib_list The list of attributes for the context. It is a
  /// list of attribute name-value pairs in which each attribute is immediately
  /// followed by the corresponding desired value. The list is terminated with
  /// PP_GRAPHICS3DATTRIB_NONE. The attrib_list may be NULL or empty
  /// (first attribute is PP_GRAPHICS3DATTRIB_NONE). If an attribute is not
  /// specified in attrib_list, then the default value is used (it is said to
  /// be specified implicitly).
  ///
  /// Attributes for the context are chosen according to an attribute-specific
  /// criteria. Attributes can be classified into two categories:
  /// - AtLeast: The attribute value in the returned context meets or exceeds
  ///            the value specified in attrib_list.
  /// - Exact: The attribute value in the returned context is equal to
  ///          the value specified in attrib_list.
  ///
  /// Attributes that can be specified in attrib_list include:
  /// - PP_GRAPHICS3DATTRIB_ALPHA_SIZE: Category: AtLeast Default: 0.
  /// - PP_GRAPHICS3DATTRIB_BLUE_SIZE: Category: AtLeast Default: 0.
  /// - PP_GRAPHICS3DATTRIB_GREEN_SIZE: Category: AtLeast Default: 0.
  /// - PP_GRAPHICS3DATTRIB_RED_SIZE: Category: AtLeast Default: 0.
  /// - PP_GRAPHICS3DATTRIB_DEPTH_SIZE: Category: AtLeast Default: 0.
  /// - PP_GRAPHICS3DATTRIB_STENCIL_SIZE: Category: AtLeast Default: 0.
  /// - PP_GRAPHICS3DATTRIB_SAMPLES: Category: AtLeast Default: 0.
  /// - PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS: Category: AtLeast Default: 0.
  /// - PP_GRAPHICS3DATTRIB_WIDTH: Category: Exact Default: 0.
  /// - PP_GRAPHICS3DATTRIB_HEIGHT: Category: Exact Default: 0.
  /// - PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR:
  ///   Category: Exact Default: Implementation defined.
  ///
  /// On failure, the object will be is_null().
  Graphics3D(const Instance* instance,
             const int32_t* attrib_list);

  /// Creates and initializes a 3D rendering context. The returned context is
  /// off-screen to start with. It must be attached to a plugin instance using
  /// Instance::BindGraphics to draw on the web page.
  ///
  /// This is identical to the 2-argument version except that this version
  /// allows sharing of resources with another context.
  ///
  /// @param[in] instance The instance that will own the new Graphics3D.
  ///
  /// @param[in] share_context Specifies the context with which all
  /// shareable data will be shared. The shareable data is defined by the
  /// client API (note that for OpenGL and OpenGL ES, shareable data excludes
  /// texture objects named 0). An arbitrary number of Graphics3D resources
  /// can share data in this fashion.
  //
  /// @param[in] attrib_list The list of attributes for the context. See the
  /// 2-argument version for more information.
  ///
  /// On failure, the object will be is_null().
  Graphics3D(const Instance* instance,
             const Graphics3D& share_context,
             const int32_t* attrib_list);

  ~Graphics3D();

  /// Retrieves the value for each attribute in attrib_list. The list
  /// has the same structure as described for the constructor.
  /// It is both input and output structure for this function.
  ///
  /// All attributes specified in PPB_Graphics3D.Create can be queried for.
  /// On failure the following error codes may be returned:
  /// - PP_ERROR_BADRESOURCE if context is invalid.
  /// - PP_ERROR_BADARGUMENT if attrib_list is NULL or any attribute in the
  ///   attrib_list is not a valid attribute.
  ///
  /// Example usage: To get the values for rgb bits in the color buffer,
  /// this function must be called as following:
  /// int attrib_list[] = {PP_GRAPHICS3DATTRIB_RED_SIZE, 0,
  ///                      PP_GRAPHICS3DATTRIB_GREEN_SIZE, 0,
  ///                      PP_GRAPHICS3DATTRIB_BLUE_SIZE, 0,
  ///                      PP_GRAPHICS3DATTRIB_NONE};
  /// GetAttribs(context, attrib_list);
  /// int red_bits = attrib_list[1];
  /// int green_bits = attrib_list[3];
  /// int blue_bits = attrib_list[5];
  int32_t GetAttribs(int32_t* attrib_list) const;

  /// Sets the values for each attribute in attrib_list. The list
  /// has the same structure as described for PPB_Graphics3D.Create.
  ///
  /// Attributes that can be specified are:
  /// - PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR
  ///
  /// On failure the following error codes may be returned:
  /// - PP_ERROR_BADRESOURCE if context is invalid.
  /// - PP_ERROR_BADARGUMENT if attrib_list is NULL or any attribute in the
  ///   attrib_list is not a valid attribute.
  int32_t SetAttribs(int32_t* attrib_list);

  /// Resizes the backing surface for the context.
  ///
  /// On failure the following error codes may be returned:
  /// - PP_ERROR_BADRESOURCE if context is invalid.
  /// - PP_ERROR_BADARGUMENT if the value specified for width or height
  ///   is less than zero.
  ///
  /// If the surface could not be resized due to insufficient resources,
  /// PP_ERROR_NOMEMORY error is returned on the next SwapBuffers callback.
  int32_t ResizeBuffers(int32_t width, int32_t height);

  /// Makes the contents of the color buffer available for compositing.
  /// This function has no effect on off-screen surfaces - ones not bound
  /// to any plugin instance. The contents of ancillary buffers are always
  /// undefined after calling SwapBuffers. The contents of the color buffer are
  /// undefined if the value of the PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR attribute
  /// of context is not PP_GRAPHICS3DATTRIB_BUFFER_PRESERVED.
  ///
  /// SwapBuffers runs in asynchronous mode. Specify a callback function and the
  /// argument for that callback function. The callback function will be
  /// executed on the calling thread after the color buffer has been composited
  /// with rest of the html page. While you are waiting for a SwapBuffers
  /// callback, additional calls to SwapBuffers will fail.
  ///
  /// Because the callback is executed (or thread unblocked) only when the
  /// plugin's current state is actually on the screen, this function provides a
  /// way to rate limit animations. By waiting until the image is on the screen
  /// before painting the next frame, you can ensure you're not generating
  /// updates faster than the screen can be updated.
  ///
  /// SwapBuffers performs an implicit flush operation on context.
  /// If the context gets into an unrecoverable error condition while
  /// processing a command, the error code will be returned as the argument
  /// for the callback. The callback may return the following error codes:
  /// - PP_ERROR_NOMEMORY
  /// - PP_ERROR_CONTEXT_LOST
  /// Note that the same error code may also be obtained by calling GetError.
  ///
  /// On failure SwapBuffers may return the following error codes:
  /// - PP_ERROR_BADRESOURCE if context is invalid.
  /// - PP_ERROR_BADARGUMENT if callback is invalid.
  int32_t SwapBuffers(const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_GRAPHICS_3D_H_

