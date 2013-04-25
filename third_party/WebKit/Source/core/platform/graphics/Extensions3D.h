/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Extensions3D_h
#define Extensions3D_h

#include "core/platform/graphics/GraphicsTypes3D.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

class GraphicsContext3DPrivate;
class ImageBuffer;

// The supported extensions are defined below.
//
// Calling any extension function not supported by the current context
// must be a no-op; in particular, it may not have side effects. In
// this situation, if the function has a return value, 0 is returned.
class Extensions3D {
public:
    ~Extensions3D();

    // Supported extensions:
    //   GL_EXT_texture_format_BGRA8888
    //   GL_EXT_read_format_bgra
    //   GL_ARB_robustness
    //   GL_ARB_texture_non_power_of_two / GL_OES_texture_npot
    //   GL_EXT_packed_depth_stencil / GL_OES_packed_depth_stencil
    //   GL_ANGLE_framebuffer_blit / GL_ANGLE_framebuffer_multisample
    //   GL_OES_texture_float
    //   GL_OES_texture_half_float
    //   GL_OES_standard_derivatives
    //   GL_OES_rgb8_rgba8
    //   GL_OES_vertex_array_object
    //   GL_OES_element_index_uint
    //   GL_ANGLE_translated_shader_source
    //   GL_ARB_texture_rectangle (only the subset required to
    //     implement IOSurface binding; it's recommended to support
    //     this only on Mac OS X to limit the amount of code dependent
    //     on this extension)
    //   GL_EXT_texture_compression_dxt1
    //   GL_EXT_texture_compression_s3tc
    //   GL_OES_compressed_ETC1_RGB8_texture
    //   GL_IMG_texture_compression_pvrtc
    //   EXT_texture_filter_anisotropic
    //   GL_EXT_debug_marker
    //   GL_CHROMIUM_copy_texture
    //   GL_CHROMIUM_flipy
    //   GL_ARB_draw_buffers / GL_EXT_draw_buffers

    //   GL_CHROMIUM_shallow_flush  : only supported if an ipc command buffer is used.
    //   GL_CHROMIUM_resource_safe  : indicating that textures/renderbuffers are always initialized before read/write.
    //   GL_CHROMIUM_strict_attribs : indicating a GL error is generated for out-of-bounds buffer accesses.
    //   GL_CHROMIUM_post_sub_buffer
    //   GL_CHROMIUM_map_sub
    //   GL_CHROMIUM_swapbuffers_complete_callback
    //   GL_CHROMIUM_rate_limit_offscreen_context
    //   GL_CHROMIUM_paint_framebuffer_canvas
    //   GL_CHROMIUM_iosurface (Mac OS X specific)
    //   GL_CHROMIUM_command_buffer_query
    //   GL_ANGLE_texture_usage
    //   GL_EXT_debug_marker
    //   GL_EXT_texture_storage
    //   GL_EXT_occlusion_query_boolean

    // Takes full name of extension; for example,
    // "GL_EXT_texture_format_BGRA8888".
    bool supports(const String&);

    // Certain OpenGL and WebGL implementations may support enabling
    // extensions lazily. This method may only be called with
    // extension names for which supports returns true.
    void ensureEnabled(const String&);

    // Takes full name of extension: for example, "GL_EXT_texture_format_BGRA8888".
    // Checks to see whether the given extension is actually enabled (see ensureEnabled).
    // Has no other side-effects.
    bool isEnabled(const String&);

    enum ExtensionsEnumType {
        // GL_EXT_texture_format_BGRA8888 enums
        BGRA_EXT = 0x80E1,

        // GL_ARB_robustness enums
        GUILTY_CONTEXT_RESET_ARB = 0x8253,
        INNOCENT_CONTEXT_RESET_ARB = 0x8254,
        UNKNOWN_CONTEXT_RESET_ARB = 0x8255,

        // GL_EXT/OES_packed_depth_stencil enums
        DEPTH24_STENCIL8 = 0x88F0,
        
        // GL_ANGLE_framebuffer_blit names
        READ_FRAMEBUFFER = 0x8CA8,
        DRAW_FRAMEBUFFER = 0x8CA9,
        DRAW_FRAMEBUFFER_BINDING = 0x8CA6, 
        READ_FRAMEBUFFER_BINDING = 0x8CAA,
        
        // GL_ANGLE_framebuffer_multisample names
        RENDERBUFFER_SAMPLES = 0x8CAB,
        FRAMEBUFFER_INCOMPLETE_MULTISAMPLE = 0x8D56,
        MAX_SAMPLES = 0x8D57,

        // GL_OES_standard_derivatives names
        FRAGMENT_SHADER_DERIVATIVE_HINT_OES = 0x8B8B,

        // GL_OES_rgb8_rgba8 names
        RGB8_OES = 0x8051,
        RGBA8_OES = 0x8058,
        
        // GL_OES_vertex_array_object names
        VERTEX_ARRAY_BINDING_OES = 0x85B5,

        // GL_ANGLE_translated_shader_source
        TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE = 0x93A0,

        // GL_ARB_texture_rectangle
        TEXTURE_RECTANGLE_ARB =  0x84F5,
        TEXTURE_BINDING_RECTANGLE_ARB = 0x84F6,

        // GL_EXT_texture_compression_dxt1
        // GL_EXT_texture_compression_s3tc
        COMPRESSED_RGB_S3TC_DXT1_EXT = 0x83F0,
        COMPRESSED_RGBA_S3TC_DXT1_EXT = 0x83F1,
        COMPRESSED_RGBA_S3TC_DXT3_EXT = 0x83F2,
        COMPRESSED_RGBA_S3TC_DXT5_EXT = 0x83F3,

        // GL_OES_compressed_ETC1_RGB8_texture
        ETC1_RGB8_OES = 0x8D64,

        // GL_IMG_texture_compression_pvrtc
        COMPRESSED_RGB_PVRTC_4BPPV1_IMG = 0x8C00,
        COMPRESSED_RGB_PVRTC_2BPPV1_IMG = 0x8C01,
        COMPRESSED_RGBA_PVRTC_4BPPV1_IMG = 0x8C02,
        COMPRESSED_RGBA_PVRTC_2BPPV1_IMG = 0x8C03,

        // GL_AMD_compressed_ATC_texture
        COMPRESSED_ATC_RGB_AMD = 0x8C92,
        COMPRESSED_ATC_RGBA_EXPLICIT_ALPHA_AMD = 0x8C93,
        COMPRESSED_ATC_RGBA_INTERPOLATED_ALPHA_AMD = 0x87EE,

        // GL_EXT_texture_filter_anisotropic
        TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE,
        MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF,

        // GL_CHROMIUM_flipy
        UNPACK_FLIP_Y_CHROMIUM = 0x9240,

        // GL_CHROMIUM_copy_texture
        UNPACK_PREMULTIPLY_ALPHA_CHROMIUM = 0x9241,
        UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM = 0x9242,

        // GL_ARB_draw_buffers / GL_EXT_draw_buffers
        MAX_DRAW_BUFFERS_EXT = 0x8824,
        DRAW_BUFFER0_EXT = 0x8825,
        DRAW_BUFFER1_EXT = 0x8826,
        DRAW_BUFFER2_EXT = 0x8827,
        DRAW_BUFFER3_EXT = 0x8828,
        DRAW_BUFFER4_EXT = 0x8829,
        DRAW_BUFFER5_EXT = 0x882A,
        DRAW_BUFFER6_EXT = 0x882B,
        DRAW_BUFFER7_EXT = 0x882C,
        DRAW_BUFFER8_EXT = 0x882D,
        DRAW_BUFFER9_EXT = 0x882E,
        DRAW_BUFFER10_EXT = 0x882F,
        DRAW_BUFFER11_EXT = 0x8830,
        DRAW_BUFFER12_EXT = 0x8831,
        DRAW_BUFFER13_EXT = 0x8832,
        DRAW_BUFFER14_EXT = 0x8833,
        DRAW_BUFFER15_EXT = 0x8834,
        MAX_COLOR_ATTACHMENTS_EXT = 0x8CDF,
        COLOR_ATTACHMENT0_EXT = 0x8CE0,
        COLOR_ATTACHMENT1_EXT = 0x8CE1,
        COLOR_ATTACHMENT2_EXT = 0x8CE2,
        COLOR_ATTACHMENT3_EXT = 0x8CE3,
        COLOR_ATTACHMENT4_EXT = 0x8CE4,
        COLOR_ATTACHMENT5_EXT = 0x8CE5,
        COLOR_ATTACHMENT6_EXT = 0x8CE6,
        COLOR_ATTACHMENT7_EXT = 0x8CE7,
        COLOR_ATTACHMENT8_EXT = 0x8CE8,
        COLOR_ATTACHMENT9_EXT = 0x8CE9,
        COLOR_ATTACHMENT10_EXT = 0x8CEA,
        COLOR_ATTACHMENT11_EXT = 0x8CEB,
        COLOR_ATTACHMENT12_EXT = 0x8CEC,
        COLOR_ATTACHMENT13_EXT = 0x8CED,
        COLOR_ATTACHMENT14_EXT = 0x8CEE,
        COLOR_ATTACHMENT15_EXT = 0x8CEF,

        // GL_OES_EGL_image_external
        GL_TEXTURE_EXTERNAL_OES = 0x8D65,

        // GL_CHROMIUM_map_sub (enums inherited from GL_ARB_vertex_buffer_object)
        READ_ONLY = 0x88B8,
        WRITE_ONLY = 0x88B9,

        // GL_ANGLE_texture_usage
        GL_TEXTURE_USAGE_ANGLE = 0x93A2,
        GL_FRAMEBUFFER_ATTACHMENT_ANGLE = 0x93A3,

        // GL_EXT_texture_storage
        BGRA8_EXT = 0x93A1,

        // GL_EXT_occlusion_query_boolean
        ANY_SAMPLES_PASSED_EXT = 0x8C2F,
        ANY_SAMPLES_PASSED_CONSERVATIVE_EXT = 0x8D6A,
        CURRENT_QUERY_EXT = 0x8865,
        QUERY_RESULT_EXT = 0x8866,
        QUERY_RESULT_AVAILABLE_EXT = 0x8867,

        // GL_CHROMIUM_command_buffer_query
        COMMANDS_ISSUED_CHROMIUM = 0x84F2
    };

    // GL_ARB_robustness
    // Note: This method's behavior differs from the GL_ARB_robustness
    // specification in the following way:
    // The implementation must not reset the error state during this call.
    // If getGraphicsResetStatusARB returns an error, it should continue
    // returning the same error. Restoring the GraphicsContext3D is handled
    // externally.
    int getGraphicsResetStatusARB();
    
    // GL_ANGLE_framebuffer_blit
    void blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter);
    
    // GL_ANGLE_framebuffer_multisample
    void renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height);
    
    // GL_OES_vertex_array_object
    Platform3DObject createVertexArrayOES();
    void deleteVertexArrayOES(Platform3DObject);
    GC3Dboolean isVertexArrayOES(Platform3DObject);
    void bindVertexArrayOES(Platform3DObject);

    // GL_ANGLE_translated_shader_source
    String getTranslatedShaderSourceANGLE(Platform3DObject);

    // GL_CHROMIUM_copy_texture
    void copyTextureCHROMIUM(GC3Denum, Platform3DObject, Platform3DObject, GC3Dint, GC3Denum);

    // EXT Robustness - uses getGraphicsResetStatusARB
    void readnPixelsEXT(int x, int y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, GC3Dsizei bufSize, void *data);
    void getnUniformfvEXT(GC3Duint program, int location, GC3Dsizei bufSize, float *params);
    void getnUniformivEXT(GC3Duint program, int location, GC3Dsizei bufSize, int *params);

    // GL_EXT_debug_marker
    void insertEventMarkerEXT(const String&);
    void pushGroupMarkerEXT(const String&);
    void popGroupMarkerEXT(void);

    // GL_ARB_draw_buffers / GL_EXT_draw_buffers
    void drawBuffersEXT(GC3Dsizei n, const GC3Denum* bufs);

    // Some helper methods to detect GPU functionality
    bool isNVIDIA() { return false; }
    bool isAMD() { return false; }
    bool isIntel() { return false; }
    String vendor() { return ""; }

    // If this method returns false then the system *definitely* does not support multisampling.
    // It does not necessarily say the system does support it - callers must attempt to construct
    // multisampled renderbuffers and check framebuffer completeness.
    // Ports should implement this to return false on configurations where it is known
    // that multisampling is not available.
    bool maySupportMultisampling() { return true; }

    // Some configurations have bugs regarding built-in functions in their OpenGL drivers
    // that must be avoided. Ports should implement this flag such configurations.
    bool requiresBuiltInFunctionEmulation() { return false; }

    // GL_CHROMIUM_map_sub
    void* mapBufferSubDataCHROMIUM(unsigned target, int offset, int size, unsigned access);
    void unmapBufferSubDataCHROMIUM(const void*);
    void* mapTexSubImage2DCHROMIUM(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, unsigned access);
    void unmapTexSubImage2DCHROMIUM(const void*);

    // GL_CHROMIUM_rate_limit_offscreen_context
    void rateLimitOffscreenContextCHROMIUM();

    // GL_CHROMIUM_paint_framebuffer_canvas
    void paintFramebufferToCanvas(int framebuffer, int width, int height, bool premultiplyAlpha, ImageBuffer*);

    // GL_CHROMIUM_iosurface
    // To avoid needing to expose extraneous enums, assumes internal format
    // RGBA, format BGRA, and type UNSIGNED_INT_8_8_8_8_REV.
    void texImageIOSurface2DCHROMIUM(unsigned target, int width, int height, uint32_t ioSurfaceId, unsigned plane);

    // GL_EXT_texture_storage
    void texStorage2DEXT(unsigned target, int levels, unsigned internalformat, int width, int height);

    // GL_EXT_occlusion_query
    Platform3DObject createQueryEXT();
    void deleteQueryEXT(Platform3DObject);
    GC3Dboolean isQueryEXT(Platform3DObject);
    void beginQueryEXT(GC3Denum, Platform3DObject);
    void endQueryEXT(GC3Denum);
    void getQueryivEXT(GC3Denum, GC3Denum, GC3Dint*);
    void getQueryObjectuivEXT(Platform3DObject, GC3Denum, GC3Duint*);

    // GL_CHROMIUM_shallow_flush
    void shallowFlushCHROMIUM();

private:
    // Instances of this class are strictly owned by the GraphicsContext3D implementation and do not
    // need to be instantiated by any other code.
    friend class GraphicsContext3DPrivate;
    explicit Extensions3D(GraphicsContext3DPrivate*);

    // Weak pointer back to GraphicsContext3DPrivate
    GraphicsContext3DPrivate* m_private;
};

} // namespace WebCore

#endif // Extensions3D_h
