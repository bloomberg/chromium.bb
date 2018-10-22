//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils.h: Queries for GL image formats.

#ifndef LIBANGLE_FORMATUTILS_H_
#define LIBANGLE_FORMATUTILS_H_

#include <stdint.h>
#include <cstddef>
#include <ostream>

#include "angle_gl.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Error.h"
#include "libANGLE/Version.h"
#include "libANGLE/angletypes.h"

namespace gl
{
struct VertexAttribute;

struct FormatType final
{
    FormatType();
    FormatType(GLenum format_, GLenum type_);
    FormatType(const FormatType &other) = default;
    FormatType &operator=(const FormatType &other) = default;

    bool operator<(const FormatType &other) const;

    GLenum format;
    GLenum type;
};

struct Type
{
    Type();

    GLuint bytes;
    GLuint bytesShift;  // Bit shift by this value to effectively divide/multiply by "bytes" in a
                        // more optimal way
    bool specialInterpretation;
};
const Type &GetTypeInfo(GLenum type);

// Information about an OpenGL internal format.  Can be keyed on the internalFormat and type
// members.
struct InternalFormat
{
    InternalFormat();
    InternalFormat(const InternalFormat &other);

    GLuint computePixelBytes(GLenum formatType) const;

    ANGLE_NO_DISCARD bool computeRowPitch(GLenum formatType,
                                          GLsizei width,
                                          GLint alignment,
                                          GLint rowLength,
                                          GLuint *resultOut) const;
    ANGLE_NO_DISCARD bool computeDepthPitch(GLsizei height,
                                            GLint imageHeight,
                                            GLuint rowPitch,
                                            GLuint *resultOut) const;
    ANGLE_NO_DISCARD bool computeDepthPitch(GLenum formatType,
                                            GLsizei width,
                                            GLsizei height,
                                            GLint alignment,
                                            GLint rowLength,
                                            GLint imageHeight,
                                            GLuint *resultOut) const;

    ANGLE_NO_DISCARD bool computeCompressedImageSize(const Extents &size, GLuint *resultOut) const;

    ANGLE_NO_DISCARD bool computeSkipBytes(GLenum formatType,
                                           GLuint rowPitch,
                                           GLuint depthPitch,
                                           const PixelStoreStateBase &state,
                                           bool is3D,
                                           GLuint *resultOut) const;

    ANGLE_NO_DISCARD bool computePackUnpackEndByte(GLenum formatType,
                                                   const Extents &size,
                                                   const PixelStoreStateBase &state,
                                                   bool is3D,
                                                   GLuint *resultOut) const;

    bool isLUMA() const;
    GLenum getReadPixelsFormat() const;
    GLenum getReadPixelsType(const Version &version) const;

    // Return true if the format is a required renderbuffer format in the given version of the core
    // spec. Note that it isn't always clear whether all the rules that apply to core required
    // renderbuffer formats also apply to additional formats added by extensions. Because of this
    // extension formats are conservatively not included.
    bool isRequiredRenderbufferFormat(const Version &version) const;

    bool operator==(const InternalFormat &other) const;
    bool operator!=(const InternalFormat &other) const;

    GLenum internalFormat;

    bool sized;
    GLenum sizedInternalFormat;

    GLuint redBits;
    GLuint greenBits;
    GLuint blueBits;

    GLuint luminanceBits;

    GLuint alphaBits;
    GLuint sharedBits;

    GLuint depthBits;
    GLuint stencilBits;

    GLuint pixelBytes;

    GLuint componentCount;

    bool compressed;
    GLuint compressedBlockWidth;
    GLuint compressedBlockHeight;

    GLenum format;
    GLenum type;

    GLenum componentType;
    GLenum colorEncoding;

    typedef bool (*SupportCheckFunction)(const Version &, const Extensions &);
    SupportCheckFunction textureSupport;
    SupportCheckFunction filterSupport;
    SupportCheckFunction textureAttachmentSupport;  // glFramebufferTexture2D
    SupportCheckFunction renderbufferSupport;       // glFramebufferRenderbuffer
};

// A "Format" wraps an InternalFormat struct, querying it from either a sized internal format or
// unsized internal format and type.
// TODO(geofflang): Remove this, it doesn't add any more information than the InternalFormat object.
struct Format
{
    // Sized types only.
    explicit Format(GLenum internalFormat);

    // Sized or unsized types.
    explicit Format(const InternalFormat &internalFormat);
    Format(GLenum internalFormat, GLenum type);

    Format(const Format &other);
    Format &operator=(const Format &other);

    bool valid() const;

    static Format Invalid();
    static bool SameSized(const Format &a, const Format &b);
    static bool EquivalentForBlit(const Format &a, const Format &b);

    friend std::ostream &operator<<(std::ostream &os, const Format &fmt);

    // This is the sized info.
    const InternalFormat *info;
};

const InternalFormat &GetSizedInternalFormatInfo(GLenum internalFormat);
const InternalFormat &GetInternalFormatInfo(GLenum internalFormat, GLenum type);

// Strip sizing information from an internal format.  Doesn't necessarily validate that the internal
// format is valid.
GLenum GetUnsizedFormat(GLenum internalFormat);

typedef std::set<GLenum> FormatSet;
const FormatSet &GetAllSizedInternalFormats();

// From the ESSL 3.00.4 spec:
// Vertex shader inputs can only be float, floating-point vectors, matrices, signed and unsigned
// integers and integer vectors. Vertex shader inputs cannot be arrays or structures.

enum AttributeType
{
    ATTRIBUTE_FLOAT,
    ATTRIBUTE_VEC2,
    ATTRIBUTE_VEC3,
    ATTRIBUTE_VEC4,
    ATTRIBUTE_INT,
    ATTRIBUTE_IVEC2,
    ATTRIBUTE_IVEC3,
    ATTRIBUTE_IVEC4,
    ATTRIBUTE_UINT,
    ATTRIBUTE_UVEC2,
    ATTRIBUTE_UVEC3,
    ATTRIBUTE_UVEC4,
    ATTRIBUTE_MAT2,
    ATTRIBUTE_MAT3,
    ATTRIBUTE_MAT4,
    ATTRIBUTE_MAT2x3,
    ATTRIBUTE_MAT2x4,
    ATTRIBUTE_MAT3x2,
    ATTRIBUTE_MAT3x4,
    ATTRIBUTE_MAT4x2,
    ATTRIBUTE_MAT4x3,
};

AttributeType GetAttributeType(GLenum enumValue);

enum VertexFormatType
{
    VERTEX_FORMAT_INVALID,
    VERTEX_FORMAT_SBYTE1,
    VERTEX_FORMAT_SBYTE1_NORM,
    VERTEX_FORMAT_SBYTE2,
    VERTEX_FORMAT_SBYTE2_NORM,
    VERTEX_FORMAT_SBYTE3,
    VERTEX_FORMAT_SBYTE3_NORM,
    VERTEX_FORMAT_SBYTE4,
    VERTEX_FORMAT_SBYTE4_NORM,
    VERTEX_FORMAT_UBYTE1,
    VERTEX_FORMAT_UBYTE1_NORM,
    VERTEX_FORMAT_UBYTE2,
    VERTEX_FORMAT_UBYTE2_NORM,
    VERTEX_FORMAT_UBYTE3,
    VERTEX_FORMAT_UBYTE3_NORM,
    VERTEX_FORMAT_UBYTE4,
    VERTEX_FORMAT_UBYTE4_NORM,
    VERTEX_FORMAT_SSHORT1,
    VERTEX_FORMAT_SSHORT1_NORM,
    VERTEX_FORMAT_SSHORT2,
    VERTEX_FORMAT_SSHORT2_NORM,
    VERTEX_FORMAT_SSHORT3,
    VERTEX_FORMAT_SSHORT3_NORM,
    VERTEX_FORMAT_SSHORT4,
    VERTEX_FORMAT_SSHORT4_NORM,
    VERTEX_FORMAT_USHORT1,
    VERTEX_FORMAT_USHORT1_NORM,
    VERTEX_FORMAT_USHORT2,
    VERTEX_FORMAT_USHORT2_NORM,
    VERTEX_FORMAT_USHORT3,
    VERTEX_FORMAT_USHORT3_NORM,
    VERTEX_FORMAT_USHORT4,
    VERTEX_FORMAT_USHORT4_NORM,
    VERTEX_FORMAT_SINT1,
    VERTEX_FORMAT_SINT1_NORM,
    VERTEX_FORMAT_SINT2,
    VERTEX_FORMAT_SINT2_NORM,
    VERTEX_FORMAT_SINT3,
    VERTEX_FORMAT_SINT3_NORM,
    VERTEX_FORMAT_SINT4,
    VERTEX_FORMAT_SINT4_NORM,
    VERTEX_FORMAT_UINT1,
    VERTEX_FORMAT_UINT1_NORM,
    VERTEX_FORMAT_UINT2,
    VERTEX_FORMAT_UINT2_NORM,
    VERTEX_FORMAT_UINT3,
    VERTEX_FORMAT_UINT3_NORM,
    VERTEX_FORMAT_UINT4,
    VERTEX_FORMAT_UINT4_NORM,
    VERTEX_FORMAT_SBYTE1_INT,
    VERTEX_FORMAT_SBYTE2_INT,
    VERTEX_FORMAT_SBYTE3_INT,
    VERTEX_FORMAT_SBYTE4_INT,
    VERTEX_FORMAT_UBYTE1_INT,
    VERTEX_FORMAT_UBYTE2_INT,
    VERTEX_FORMAT_UBYTE3_INT,
    VERTEX_FORMAT_UBYTE4_INT,
    VERTEX_FORMAT_SSHORT1_INT,
    VERTEX_FORMAT_SSHORT2_INT,
    VERTEX_FORMAT_SSHORT3_INT,
    VERTEX_FORMAT_SSHORT4_INT,
    VERTEX_FORMAT_USHORT1_INT,
    VERTEX_FORMAT_USHORT2_INT,
    VERTEX_FORMAT_USHORT3_INT,
    VERTEX_FORMAT_USHORT4_INT,
    VERTEX_FORMAT_SINT1_INT,
    VERTEX_FORMAT_SINT2_INT,
    VERTEX_FORMAT_SINT3_INT,
    VERTEX_FORMAT_SINT4_INT,
    VERTEX_FORMAT_UINT1_INT,
    VERTEX_FORMAT_UINT2_INT,
    VERTEX_FORMAT_UINT3_INT,
    VERTEX_FORMAT_UINT4_INT,
    VERTEX_FORMAT_FIXED1,
    VERTEX_FORMAT_FIXED2,
    VERTEX_FORMAT_FIXED3,
    VERTEX_FORMAT_FIXED4,
    VERTEX_FORMAT_HALF1,
    VERTEX_FORMAT_HALF2,
    VERTEX_FORMAT_HALF3,
    VERTEX_FORMAT_HALF4,
    VERTEX_FORMAT_FLOAT1,
    VERTEX_FORMAT_FLOAT2,
    VERTEX_FORMAT_FLOAT3,
    VERTEX_FORMAT_FLOAT4,
    VERTEX_FORMAT_SINT210,
    VERTEX_FORMAT_UINT210,
    VERTEX_FORMAT_SINT210_NORM,
    VERTEX_FORMAT_UINT210_NORM,
    VERTEX_FORMAT_SINT210_INT,
    VERTEX_FORMAT_UINT210_INT,
};

typedef std::vector<VertexFormatType> InputLayout;

struct VertexFormat : private angle::NonCopyable
{
    VertexFormat(GLenum typeIn, GLboolean normalizedIn, GLuint componentsIn, bool pureIntegerIn);

    GLenum type;
    GLboolean normalized;
    GLuint components;
    bool pureInteger;
};

angle::FormatID GetVertexFormatID(GLenum type,
                                  GLboolean normalized,
                                  GLuint components,
                                  bool pureInteger);
angle::FormatID GetVertexFormatID(const VertexAttribute &attrib);
VertexFormatType GetVertexFormatType(GLenum type,
                                     GLboolean normalized,
                                     GLuint components,
                                     bool pureInteger);
VertexFormatType GetVertexFormatType(const VertexAttribute &attrib);
VertexFormatType GetVertexFormatType(const VertexAttribute &attrib, GLenum currentValueType);
const VertexFormat &GetVertexFormatFromType(VertexFormatType vertexFormatType);
size_t GetVertexFormatTypeSize(VertexFormatType vertexFormatType);

// Check if an internal format is ever valid in ES3.  Makes no checks about support for a specific
// context.
bool ValidES3InternalFormat(GLenum internalFormat);

// Implemented in format_map_autogen.cpp
bool ValidES3Format(GLenum format);
bool ValidES3Type(GLenum type);
bool ValidES3FormatCombination(GLenum format, GLenum type, GLenum internalFormat);

// Implemented in es3_copy_conversion_table_autogen.cpp
bool ValidES3CopyConversion(GLenum textureFormat, GLenum framebufferFormat);

}  // namespace gl

#endif  // LIBANGLE_FORMATUTILS_H_
