/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_C_DEV_PP_GRAPHICS_3D_DEV_H_
#define PPAPI_C_DEV_PP_GRAPHICS_3D_DEV_H_

#include "ppapi/c/pp_stdint.h"

// TODO(alokp): Using PP_Graphics3D prefix is making these enum names rather
// long. Can we just use PP_GL? It will be a nice short replacement of EGL.
// In which case we should rename associated classes as - PP_GL, PP_GLContext,
// PP_GLContext, PP_GLSurface, PP_GLConfig, and PP_OpenGLES2.
//
// Another option is to rename Surface3D and Context3D to Graphics3DSurface
// and Graphics3DContext respectively But this does not make the names
// any shorter.

enum PP_Graphics3DError_Dev {
  PP_GRAPHICS3DERROR_BAD_ACCESS = 0x3002,
  PP_GRAPHICS3DERROR_BAD_ATTRIBUTE = 0x3004,
  PP_GRAPHICS3DERROR_BAD_CONFIG = 0x3005,
  PP_GRAPHICS3DERROR_BAD_CONTEXT = 0x3006,
  PP_GRAPHICS3DERROR_BAD_MATCH = 0x3009,
  PP_GRAPHICS3DERROR_BAD_SURFACE = 0x300D,
  PP_GRAPHICS3DERROR_CONTEXT_LOST = 0x300E
};

enum PP_Graphics3DString_Dev {
  PP_GRAPHICS3DSTRING_VENDOR = 0x3053,
  PP_GRAPHICS3DSTRING_VERSION = 0x3054,
  // Which extensions are supported.
  PP_GRAPHICS3DSTRING_EXTENSIONS = 0x3055,
  // Which client rendering APIs are supported.
  PP_GRAPHICS3DSTRING_CLIENT_APIS = 0x308D
};

enum PP_Graphics3DAttrib_Dev {
  // Total color component bits in the color buffer.
  PP_GRAPHICS3DATTRIB_BUFFER_SIZE = 0x3020,
  // Bits of Alpha in the color buffer.
  PP_GRAPHICS3DATTRIB_ALPHA_SIZE = 0x3021,
  // Bits of Blue in the color buffer.
  PP_GRAPHICS3DATTRIB_BLUE_SIZE = 0x3022,
  // Bits of Green in the color buffer.
  PP_GRAPHICS3DATTRIB_GREEN_SIZE = 0x3023,
  // Bits of Red in the color buffer.
  PP_GRAPHICS3DATTRIB_RED_SIZE = 0x3024,
  // Bits of Z in the depth buffer.
  PP_GRAPHICS3DATTRIB_DEPTH_SIZE = 0x3025,
  // Bits of Stencil in the stencil buffer.
  PP_GRAPHICS3DATTRIB_STENCIL_SIZE = 0x3026,
  // Any caveats for the configuration.
  PP_GRAPHICS3DATTRIB_CONFIG_CAVEAT = 0x3027,
  // Unique EGLConfig identifier.
  PP_GRAPHICS3DATTRIB_CONFIG_ID = 0x3028,
  // Maximum height of surface.
  PP_GRAPHICS3DATTRIB_MAX_SURFACE_HEIGHT = 0x302A,
  // Maximum size of surface.
  PP_GRAPHICS3DATTRIB_MAX_SURFACE_PIXELS = 0x302B,
  // Maximum width of surface.
  PP_GRAPHICS3DATTRIB_MAX_SURFACE_WIDTH = 0x302C,
  // Number of samples per pixel.
  PP_GRAPHICS3DATTRIB_SAMPLES = 0x3031,
  // Number of multisample buffers.
  PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS = 0x3032,
  // Which types of EGL surfaces are supported.
  PP_GRAPHICS3DATTRIB_SURFACE_TYPE = 0x3033,
  // Type of transparency supported.
  PP_GRAPHICS3DATTRIB_TRANSPARENT_TYPE = 0x3034,
  // Transparent blue value.
  PP_GRAPHICS3DATTRIB_TRANSPARENT_BLUE_VALUE = 0x3035,
  // Transparent green value.
  PP_GRAPHICS3DATTRIB_TRANSPARENT_GREEN_VALUE = 0x3036,
  // Transparent red value.
  PP_GRAPHICS3DATTRIB_TRANSPARENT_RED_VALUE = 0x3037,
  // Attrib list terminator.
  PP_GRAPHICS3DATTRIB_NONE = 0x3038,
  // TODO(alokp): Find out if we can support swap intervals. Remove them if not.
  // Minimum swap interval.
  PP_GRAPHICS3DATTRIB_MIN_SWAP_INTERVAL = 0x303B,
  // Maximum swap interval.
  PP_GRAPHICS3DATTRIB_MAX_SWAP_INTERVAL = 0x303C,
  // Bits of Luminance in the color buffer.
  PP_GRAPHICS3DATTRIB_LUMINANCE_SIZE = 0x303D,
  // Bits of Alpha Mask in the mask buffer.
  PP_GRAPHICS3DATTRIB_ALPHA_MASK_SIZE = 0x303E,
  // Color buffer type.
  PP_GRAPHICS3DATTRIB_COLOR_BUFFER_TYPE = 0x303F,
  // Which client APIs are supported.
  PP_GRAPHICS3DATTRIB_RENDERABLE_TYPE = 0x3040,
  // Whether contexts created with this config are conformant.
  PP_GRAPHICS3DATTRIB_CONFORMANT = 0x3042,

  // Surface-specific attributes.
  // Height of surface in pixels.
  PP_GRAPHICS3DATTRIB_HEIGHT = 0x3056,
  // Width of surface in pixels.
  PP_GRAPHICS3DATTRIB_WIDTH = 0x3057,
  // If true, largest possible surface is created.
  PP_GRAPHICS3DATTRIB_LARGEST_SURFACE = 0x3058,
  // The buffer which client API renders to.
  PP_GRAPHICS3DATTRIB_RENDER_BUFFER = 0x3086,
  // Specifies the effect on the color buffer of posting a surface
  // with SwapBuffers.
  PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR = 0x3093,
  // Specifies the filter to use when resolving the multisample buffer.
  PP_GRAPHICS3DATTRIB_MULTISAMPLE_RESOLVE = 0x3099,

  // Context-specific attributes.
  // Which client API the context supports.
  PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_TYPE = 0x3097,
  // Version of OpenGL ES supported by a context.
  // An attribute value of 1 specifies OpenGL ES 1.x.
  // An attribute value of 2 specifies OpenGL ES 2.x.
  // The default value for PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_VERSION is 1.
  // This attribute is only meaningful for an OpenGL ES context.
  PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_VERSION = 0x3098
};

enum PP_Graphics3DAttribValue_Dev {
  PP_GRAPHICS3DATTRIBVALUE_NONE = 0x3038,

  // PP_GRAPHICS3DATTRIB_CONFIG_CAVEAT values.
  PP_GRAPHICS3DATTRIBVALUE_SLOW_CONFIG = 0x3050,
  PP_GRAPHICS3DATTRIBVALUE_NON_CONFORMANT_CONFIG = 0x3051,

  // PP_GRAPHICS3DATTRIB_TRANSPARENT_TYPE values.
  PP_GRAPHICS3DATTRIBVALUE_TRANSPARENT_RGB = 0x3052,

  // PP_GRAPHICS3DATTRIB_COLOR_BUFFER_TYPE values.
  PP_GRAPHICS3DATTRIBVALUE_RGB_BUFFER = 0x308E,
  PP_GRAPHICS3DATTRIBVALUE_LUMINANCE_BUFFER = 0x308F,

  // PP_GRAPHICS3DATTRIB_SURFACE_TYPE mask bits.
  // Indicates if box filter (PP_GRAPHICS3DATTRIBVALUE_MULTISAMPLE_RESOLVE_BOX)
  // for resolving the multisample buffer is supported.
  PP_GRAPHICS3DATTRIBVALUE_MULTISAMPLE_RESOLVE_BOX_BIT = 0x0200,
  PP_GRAPHICS3DATTRIBVALUE_SWAP_BEHAVIOR_PRESERVED_BIT = 0x0400,

  // PP_GRAPHICS3DATTRIB_RENDERABLE_TYPE mask bits.
  PP_GRAPHICS3DATTRIBVALUE_OPENGL_ES_BIT = 0x0001,
  PP_GRAPHICS3DATTRIBVALUE_OPENGL_ES2_BIT = 0x0004,
  PP_GRAPHICS3DATTRIBVALUE_OPENGL_BIT = 0x0008,

  // PP_GRAPHICS3DATTRIB_RENDER_BUFFER values.
  // The default value is PP_GRAPHICS3DATTRIBVALUE_BACK_BUFFER.
  // Surface contains two color buffers and client APIs render into
  // the back buffer.
  PP_GRAPHICS3DATTRIBVALUE_BACK_BUFFER = 0x3084,
  // Surface contains a single color buffer.
  PP_GRAPHICS3DATTRIBVALUE_SINGLE_BUFFER = 0x3085,

  // PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR values.
  // The initial value is chosen by the implementation.
  // Indicates that color buffer contents are unaffected.
  PP_GRAPHICS3DATTRIBVALUE_BUFFER_PRESERVED = 0x3094,
  // Indicates that color buffer contents may be destroyed or changed.
  PP_GRAPHICS3DATTRIBVALUE_BUFFER_DESTROYED = 0x3095,

  // PP_GRAPHICS3DATTRIB_MULTISAMPLE_RESOLVE values.
  // The default value is PP_GRAPHICS3DATTRIBVALUE_MULTISAMPLE_RESOLVE_DEFAULT.
  // The default implementation-defined filtering method.
  PP_GRAPHICS3DATTRIBVALUE_MULTISAMPLE_RESOLVE_DEFAULT = 0x309A,
  // One-pixel wide box filter placing equal weighting on all
  // multisample values.
  PP_GRAPHICS3DATTRIBVALUE_MULTISAMPLE_RESOLVE_BOX = 0x309B,

  // PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_TYPE values.
  PP_GRAPHICS3DATTRIBVALUE_OPENGL_ES_API = 0x30A0,
  PP_GRAPHICS3DATTRIBVALUE_OPENGL_API = 0x30A2
};

typedef int32_t PP_Config3D_Dev;

#endif  // PPAPI_C_DEV_PP_GRAPHICS_3D_DEV_H_
