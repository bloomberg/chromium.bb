// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PackedGLEnums.cpp:
//   Declares ANGLE-specific enums classes for GLEnum and functions operating
//   on them.

#include "common/PackedEnums.h"

#include "common/utilities.h"

namespace gl
{

TextureType TextureTargetToType(TextureTarget target)
{
    switch (target)
    {
        case TextureTarget::CubeMapNegativeX:
        case TextureTarget::CubeMapNegativeY:
        case TextureTarget::CubeMapNegativeZ:
        case TextureTarget::CubeMapPositiveX:
        case TextureTarget::CubeMapPositiveY:
        case TextureTarget::CubeMapPositiveZ:
            return TextureType::CubeMap;
        case TextureTarget::External:
            return TextureType::External;
        case TextureTarget::Rectangle:
            return TextureType::Rectangle;
        case TextureTarget::_2D:
            return TextureType::_2D;
        case TextureTarget::_2DArray:
            return TextureType::_2DArray;
        case TextureTarget::_2DMultisample:
            return TextureType::_2DMultisample;
        case TextureTarget::_2DMultisampleArray:
            return TextureType::_2DMultisampleArray;
        case TextureTarget::_3D:
            return TextureType::_3D;
        default:
            UNREACHABLE();
            return TextureType::InvalidEnum;
    }
}

bool IsCubeMapFaceTarget(TextureTarget target)
{
    return TextureTargetToType(target) == TextureType::CubeMap;
}

TextureTarget NonCubeTextureTypeToTarget(TextureType type)
{
    switch (type)
    {
        case TextureType::External:
            return TextureTarget::External;
        case TextureType::Rectangle:
            return TextureTarget::Rectangle;
        case TextureType::_2D:
            return TextureTarget::_2D;
        case TextureType::_2DArray:
            return TextureTarget::_2DArray;
        case TextureType::_2DMultisample:
            return TextureTarget::_2DMultisample;
        case TextureType::_2DMultisampleArray:
            return TextureTarget::_2DMultisampleArray;
        case TextureType::_3D:
            return TextureTarget::_3D;
        default:
            UNREACHABLE();
            return TextureTarget::InvalidEnum;
    }
}

// Check that we can do arithmetic on TextureTarget to convert from / to cube map faces
static_assert(static_cast<uint8_t>(TextureTarget::CubeMapNegativeX) -
                      static_cast<uint8_t>(TextureTarget::CubeMapPositiveX) ==
                  1u,
              "");
static_assert(static_cast<uint8_t>(TextureTarget::CubeMapPositiveY) -
                      static_cast<uint8_t>(TextureTarget::CubeMapPositiveX) ==
                  2u,
              "");
static_assert(static_cast<uint8_t>(TextureTarget::CubeMapNegativeY) -
                      static_cast<uint8_t>(TextureTarget::CubeMapPositiveX) ==
                  3u,
              "");
static_assert(static_cast<uint8_t>(TextureTarget::CubeMapPositiveZ) -
                      static_cast<uint8_t>(TextureTarget::CubeMapPositiveX) ==
                  4u,
              "");
static_assert(static_cast<uint8_t>(TextureTarget::CubeMapNegativeZ) -
                      static_cast<uint8_t>(TextureTarget::CubeMapPositiveX) ==
                  5u,
              "");

TextureTarget CubeFaceIndexToTextureTarget(size_t face)
{
    ASSERT(face < 6u);
    return static_cast<TextureTarget>(static_cast<uint8_t>(TextureTarget::CubeMapPositiveX) + face);
}

size_t CubeMapTextureTargetToFaceIndex(TextureTarget target)
{
    ASSERT(IsCubeMapFaceTarget(target));
    return static_cast<uint8_t>(target) - static_cast<uint8_t>(TextureTarget::CubeMapPositiveX);
}

TextureType SamplerTypeToTextureType(GLenum samplerType)
{
    switch (samplerType)
    {
        case GL_SAMPLER_2D:
        case GL_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_SAMPLER_2D_SHADOW:
            return TextureType::_2D;

        case GL_SAMPLER_EXTERNAL_OES:
            return TextureType::External;

        case GL_SAMPLER_CUBE:
        case GL_INT_SAMPLER_CUBE:
        case GL_UNSIGNED_INT_SAMPLER_CUBE:
        case GL_SAMPLER_CUBE_SHADOW:
            return TextureType::CubeMap;

        case GL_SAMPLER_2D_ARRAY:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
            return TextureType::_2DArray;

        case GL_SAMPLER_3D:
        case GL_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_3D:
            return TextureType::_3D;

        case GL_SAMPLER_2D_MULTISAMPLE:
        case GL_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
            return TextureType::_2DMultisample;

        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
            return TextureType::_2DMultisampleArray;

        case GL_SAMPLER_2D_RECT_ANGLE:
            return TextureType::Rectangle;

        default:
            UNREACHABLE();
            return TextureType::InvalidEnum;
    }
}

bool IsMultisampled(gl::TextureType type)
{
    switch (type)
    {
        case gl::TextureType::_2DMultisample:
        case gl::TextureType::_2DMultisampleArray:
            return true;
        default:
            return false;
    }
}

}  // namespace gl

namespace egl
{
MessageType ErrorCodeToMessageType(EGLint errorCode)
{
    switch (errorCode)
    {
        case EGL_BAD_ALLOC:
        case EGL_CONTEXT_LOST:
        case EGL_NOT_INITIALIZED:
            return MessageType::Critical;

        case EGL_BAD_ACCESS:
        case EGL_BAD_ATTRIBUTE:
        case EGL_BAD_CONFIG:
        case EGL_BAD_CONTEXT:
        case EGL_BAD_CURRENT_SURFACE:
        case EGL_BAD_DISPLAY:
        case EGL_BAD_MATCH:
        case EGL_BAD_NATIVE_PIXMAP:
        case EGL_BAD_NATIVE_WINDOW:
        case EGL_BAD_PARAMETER:
        case EGL_BAD_SURFACE:
        case EGL_BAD_STREAM_KHR:
        case EGL_BAD_STATE_KHR:
        case EGL_BAD_DEVICE_EXT:
            return MessageType::Error;

        case EGL_SUCCESS:
        default:
            UNREACHABLE();
            return MessageType::InvalidEnum;
    }
}
}  // namespace egl

namespace egl_gl
{

gl::TextureTarget EGLCubeMapTargetToCubeMapTarget(EGLenum eglTarget)
{
    ASSERT(egl::IsCubeMapTextureTarget(eglTarget));
    return gl::CubeFaceIndexToTextureTarget(egl::CubeMapTextureTargetToLayerIndex(eglTarget));
}

gl::TextureTarget EGLImageTargetToTextureTarget(EGLenum eglTarget)
{
    switch (eglTarget)
    {
        case EGL_GL_TEXTURE_2D_KHR:
            return gl::TextureTarget::_2D;

        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
            return EGLCubeMapTargetToCubeMapTarget(eglTarget);

        case EGL_GL_TEXTURE_3D_KHR:
            return gl::TextureTarget::_3D;

        default:
            UNREACHABLE();
            return gl::TextureTarget::InvalidEnum;
    }
}

gl::TextureType EGLTextureTargetToTextureType(EGLenum eglTarget)
{
    switch (eglTarget)
    {
        case EGL_TEXTURE_2D:
            return gl::TextureType::_2D;

        case EGL_TEXTURE_RECTANGLE_ANGLE:
            return gl::TextureType::Rectangle;

        default:
            UNREACHABLE();
            return gl::TextureType::InvalidEnum;
    }
}

}  // namespace egl_gl
