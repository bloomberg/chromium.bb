/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/cpu/arm/GraphicsContext3DNEON.h"

#include "SkTypes.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/CheckedInt.h"
#include "core/platform/chromium/support/GraphicsContext3DPrivate.h"
#include "core/platform/graphics/Extensions3D.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/ImageObserver.h"
#include "core/platform/graphics/gpu/DrawingBuffer.h"
#include "core/platform/image-decoders/ImageDecoder.h"

#include <public/Platform.h>
#include <public/WebGraphicsContext3D.h>
#include <wtf/ArrayBufferView.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace {

GraphicsContext3D::DataFormat getDataFormat(GC3Denum destinationFormat, GC3Denum destinationType)
{
    GraphicsContext3D::DataFormat dstFormat = GraphicsContext3D::DataFormatRGBA8;
    switch (destinationType) {
    case GraphicsContext3D::UNSIGNED_BYTE:
        switch (destinationFormat) {
        case GraphicsContext3D::RGB:
            dstFormat = GraphicsContext3D::DataFormatRGB8;
            break;
        case GraphicsContext3D::RGBA:
            dstFormat = GraphicsContext3D::DataFormatRGBA8;
            break;
        case GraphicsContext3D::ALPHA:
            dstFormat = GraphicsContext3D::DataFormatA8;
            break;
        case GraphicsContext3D::LUMINANCE:
            dstFormat = GraphicsContext3D::DataFormatR8;
            break;
        case GraphicsContext3D::LUMINANCE_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatRA8;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
        dstFormat = GraphicsContext3D::DataFormatRGBA4444;
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        dstFormat = GraphicsContext3D::DataFormatRGBA5551;
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
        dstFormat = GraphicsContext3D::DataFormatRGB565;
        break;
    case GraphicsContext3D::HALF_FLOAT_OES: // OES_texture_half_float
        switch (destinationFormat) {
        case GraphicsContext3D::RGB:
            dstFormat = GraphicsContext3D::DataFormatRGB16F;
            break;
        case GraphicsContext3D::RGBA:
            dstFormat = GraphicsContext3D::DataFormatRGBA16F;
            break;
        case GraphicsContext3D::ALPHA:
            dstFormat = GraphicsContext3D::DataFormatA16F;
            break;
        case GraphicsContext3D::LUMINANCE:
            dstFormat = GraphicsContext3D::DataFormatR16F;
            break;
        case GraphicsContext3D::LUMINANCE_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatRA16F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContext3D::FLOAT: // OES_texture_float
        switch (destinationFormat) {
        case GraphicsContext3D::RGB:
            dstFormat = GraphicsContext3D::DataFormatRGB32F;
            break;
        case GraphicsContext3D::RGBA:
            dstFormat = GraphicsContext3D::DataFormatRGBA32F;
            break;
        case GraphicsContext3D::ALPHA:
            dstFormat = GraphicsContext3D::DataFormatA32F;
            break;
        case GraphicsContext3D::LUMINANCE:
            dstFormat = GraphicsContext3D::DataFormatR32F;
            break;
        case GraphicsContext3D::LUMINANCE_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatRA32F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return dstFormat;
}

void getDrawingParameters(DrawingBuffer* drawingBuffer, WebKit::WebGraphicsContext3D* graphicsContext3D,
                          Platform3DObject* frameBufferId, int* width, int* height)
{
    if (drawingBuffer) {
        *frameBufferId = drawingBuffer->framebuffer();
        *width = drawingBuffer->size().width();
        *height = drawingBuffer->size().height();
    } else {
        *frameBufferId = 0;
        *width = graphicsContext3D->width();
        *height = graphicsContext3D->height();
    }
}

// Following Float to Half-Float converion code is from the implementation of ftp://www.fox-toolkit.org/pub/fasthalffloatconversion.pdf,
// "Fast Half Float Conversions" by Jeroen van der Zijp, November 2008 (Revised September 2010).
// Specially, the basetable[512] and shifttable[512] are generated as follows:
/*
unsigned short basetable[512];
unsigned char shifttable[512];

void generatetables(){
    unsigned int i;
    int e;
    for (i = 0; i < 256; ++i){
        e = i - 127;
        if (e < -24){ // Very small numbers map to zero
            basetable[i | 0x000] = 0x0000;
            basetable[i | 0x100] = 0x8000;
            shifttable[i | 0x000] = 24;
            shifttable[i | 0x100] = 24;
        }
        else if (e < -14) { // Small numbers map to denorms
            basetable[i | 0x000] = (0x0400>>(-e-14));
            basetable[i | 0x100] = (0x0400>>(-e-14)) | 0x8000;
            shifttable[i | 0x000] = -e-1;
            shifttable[i | 0x100] = -e-1;
        }
        else if (e <= 15){ // Normal numbers just lose precision
            basetable[i | 0x000] = ((e+15)<<10);
            basetable[i| 0x100] = ((e+15)<<10) | 0x8000;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
        }
        else if (e<128){ // Large numbers map to Infinity
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 24;
            shifttable[i|0x100] = 24;
        }
        else { // Infinity and NaN's stay Infinity and NaN's
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
       }
    }
}
*/

unsigned short baseTable[512] = {
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      1,      2,      4,      8,      16,     32,     64,     128,    256,
512,    1024,   2048,   3072,   4096,   5120,   6144,   7168,   8192,   9216,   10240,  11264,  12288,  13312,  14336,  15360,
16384,  17408,  18432,  19456,  20480,  21504,  22528,  23552,  24576,  25600,  26624,  27648,  28672,  29696,  30720,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32769,  32770,  32772,  32776,  32784,  32800,  32832,  32896,  33024,
33280,  33792,  34816,  35840,  36864,  37888,  38912,  39936,  40960,  41984,  43008,  44032,  45056,  46080,  47104,  48128,
49152,  50176,  51200,  52224,  53248,  54272,  55296,  56320,  57344,  58368,  59392,  60416,  61440,  62464,  63488,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512
};

unsigned char shiftTable[512] = {
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     23,     22,     21,     20,     19,     18,     17,     16,     15,
14,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,
13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     13,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     23,     22,     21,     20,     19,     18,     17,     16,     15,
14,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,
13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     13
};

unsigned short convertFloatToHalfFloat(float f)
{
    unsigned temp = *(reinterpret_cast<unsigned *>(&f));
    unsigned signexp = (temp >> 23) & 0x1ff;
    return baseTable[signexp] + ((temp & 0x007fffff) >> shiftTable[signexp]);
}

} // anonymous namespace

// Macros to assist in delegating from GraphicsContext3D to
// WebGraphicsContext3D.

#define DELEGATE_TO_WEBCONTEXT(name) \
void GraphicsContext3D::name() \
{ \
    m_private->webContext()->name(); \
}

#define DELEGATE_TO_WEBCONTEXT_R(name, rt) \
rt GraphicsContext3D::name() \
{ \
    return m_private->webContext()->name(); \
}

#define DELEGATE_TO_WEBCONTEXT_1(name, t1) \
void GraphicsContext3D::name(t1 a1) \
{ \
    m_private->webContext()->name(a1); \
}

#define DELEGATE_TO_WEBCONTEXT_1R(name, t1, rt) \
rt GraphicsContext3D::name(t1 a1) \
{ \
    return m_private->webContext()->name(a1); \
}

#define DELEGATE_TO_WEBCONTEXT_2(name, t1, t2) \
void GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    m_private->webContext()->name(a1, a2); \
}

#define DELEGATE_TO_WEBCONTEXT_2R(name, t1, t2, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2) \
{ \
    return m_private->webContext()->name(a1, a2); \
}

#define DELEGATE_TO_WEBCONTEXT_3(name, t1, t2, t3) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3) \
{ \
    m_private->webContext()->name(a1, a2, a3); \
}

#define DELEGATE_TO_WEBCONTEXT_3R(name, t1, t2, t3, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3) \
{ \
    return m_private->webContext()->name(a1, a2, a3); \
}

#define DELEGATE_TO_WEBCONTEXT_4(name, t1, t2, t3, t4) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4); \
}

#define DELEGATE_TO_WEBCONTEXT_4R(name, t1, t2, t3, t4, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4) \
{ \
    return m_private->webContext()->name(a1, a2, a3, a4); \
}

#define DELEGATE_TO_WEBCONTEXT_5(name, t1, t2, t3, t4, t5) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5); \
}

#define DELEGATE_TO_WEBCONTEXT_6(name, t1, t2, t3, t4, t5, t6) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5, a6); \
}

#define DELEGATE_TO_WEBCONTEXT_6R(name, t1, t2, t3, t4, t5, t6, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
    return m_private->webContext()->name(a1, a2, a3, a4, a5, a6); \
}

#define DELEGATE_TO_WEBCONTEXT_7(name, t1, t2, t3, t4, t5, t6, t7) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7); \
}

#define DELEGATE_TO_WEBCONTEXT_7R(name, t1, t2, t3, t4, t5, t6, t7, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{ \
    return m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7); \
}

#define DELEGATE_TO_WEBCONTEXT_8(name, t1, t2, t3, t4, t5, t6, t7, t8) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7, a8); \
}

#define DELEGATE_TO_WEBCONTEXT_9(name, t1, t2, t3, t4, t5, t6, t7, t8, t9) \
void GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7, a8, a9); \
}

#define DELEGATE_TO_WEBCONTEXT_9R(name, t1, t2, t3, t4, t5, t6, t7, t8, t9, rt) \
rt GraphicsContext3D::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{ \
    return m_private->webContext()->name(a1, a2, a3, a4, a5, a6, a7, a8, a9); \
}

GraphicsContext3D::GraphicsContext3D()
{
}

GraphicsContext3D::~GraphicsContext3D()
{
    m_private->setContextLostCallback(nullptr);
    m_private->setErrorMessageCallback(nullptr);
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback> callback)
{
    m_private->setContextLostCallback(callback);
}

void GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<GraphicsContext3D::ErrorMessageCallback> callback)
{
    m_private->setErrorMessageCallback(callback);
}

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs)
{
    WebKit::WebGraphicsContext3D::Attributes webAttributes;
    webAttributes.alpha = attrs.alpha;
    webAttributes.depth = attrs.depth;
    webAttributes.stencil = attrs.stencil;
    webAttributes.antialias = attrs.antialias;
    webAttributes.premultipliedAlpha = attrs.premultipliedAlpha;
    webAttributes.noExtensions = attrs.noExtensions;
    webAttributes.shareResources = attrs.shareResources;
    webAttributes.preferDiscreteGPU = attrs.preferDiscreteGPU;
    webAttributes.topDocumentURL = attrs.topDocumentURL.string();

    OwnPtr<WebKit::WebGraphicsContext3D> webContext = adoptPtr(WebKit::Platform::current()->createOffscreenGraphicsContext3D(webAttributes));
    if (!webContext)
        return 0;

    return GraphicsContext3DPrivate::createGraphicsContextFromWebContext(webContext.release(), attrs.preserveDrawingBuffer);
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_private->webContext();
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_private->webContext()->getPlatformTextureId();
}

GrContext* GraphicsContext3D::grContext()
{
    return m_private->grContext();
}

PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return 0;
}

DELEGATE_TO_WEBCONTEXT_R(makeContextCurrent, bool)
DELEGATE_TO_WEBCONTEXT(prepareTexture)

bool GraphicsContext3D::isGLES2Compliant() const
{
    return m_private->webContext()->isGLES2Compliant();
}

bool GraphicsContext3D::isResourceSafe()
{
    return m_private->isResourceSafe();
}

DELEGATE_TO_WEBCONTEXT_1(activeTexture, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(attachShader, Platform3DObject, Platform3DObject)

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    m_private->webContext()->bindAttribLocation(program, index, name.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_2(bindBuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(bindFramebuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(bindRenderbuffer, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(bindTexture, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_4(blendColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_1(blendEquation, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(blendEquationSeparate, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(blendFunc, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_4(blendFuncSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    bufferData(target, size, 0, usage);
}

DELEGATE_TO_WEBCONTEXT_4(bufferData, GC3Denum, GC3Dsizeiptr, const void*, GC3Denum)
DELEGATE_TO_WEBCONTEXT_4(bufferSubData, GC3Denum, GC3Dintptr, GC3Dsizeiptr, const void*)

DELEGATE_TO_WEBCONTEXT_1R(checkFramebufferStatus, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(clear, GC3Dbitfield)
DELEGATE_TO_WEBCONTEXT_4(clearColor, GC3Dclampf, GC3Dclampf, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_1(clearDepth, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_1(clearStencil, GC3Dint)
DELEGATE_TO_WEBCONTEXT_4(colorMask, GC3Dboolean, GC3Dboolean, GC3Dboolean, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1(compileShader, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_8(compressedTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, const void*)
DELEGATE_TO_WEBCONTEXT_9(compressedTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Denum, GC3Dsizei, const void*)
DELEGATE_TO_WEBCONTEXT_8(copyTexImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Dint)
DELEGATE_TO_WEBCONTEXT_8(copyTexSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_WEBCONTEXT_1(cullFace, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(depthFunc, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(depthMask, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_2(depthRange, GC3Dclampf, GC3Dclampf)
DELEGATE_TO_WEBCONTEXT_2(detachShader, Platform3DObject, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(disable, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(disableVertexAttribArray, GC3Duint)
DELEGATE_TO_WEBCONTEXT_3(drawArrays, GC3Denum, GC3Dint, GC3Dsizei)
DELEGATE_TO_WEBCONTEXT_4(drawElements, GC3Denum, GC3Dsizei, GC3Denum, GC3Dintptr)

DELEGATE_TO_WEBCONTEXT_1(enable, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(enableVertexAttribArray, GC3Duint)
DELEGATE_TO_WEBCONTEXT(finish)
DELEGATE_TO_WEBCONTEXT(flush)
DELEGATE_TO_WEBCONTEXT_4(framebufferRenderbuffer, GC3Denum, GC3Denum, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_5(framebufferTexture2D, GC3Denum, GC3Denum, GC3Denum, Platform3DObject, GC3Dint)
DELEGATE_TO_WEBCONTEXT_1(frontFace, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1(generateMipmap, GC3Denum)

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_private->webContext()->getActiveAttrib(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    WebKit::WebGraphicsContext3D::ActiveInfo webInfo;
    if (!m_private->webContext()->getActiveUniform(program, index, webInfo))
        return false;
    info.name = webInfo.name;
    info.type = webInfo.type;
    info.size = webInfo.size;
    return true;
}

DELEGATE_TO_WEBCONTEXT_4(getAttachedShaders, Platform3DObject, GC3Dsizei, GC3Dsizei*, Platform3DObject*)

GC3Dint GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    return m_private->webContext()->getAttribLocation(program, name.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_2(getBooleanv, GC3Denum, GC3Dboolean*)
DELEGATE_TO_WEBCONTEXT_3(getBufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    WebKit::WebGraphicsContext3D::Attributes webAttributes = m_private->webContext()->getContextAttributes();
    GraphicsContext3D::Attributes attributes;
    attributes.alpha = webAttributes.alpha;
    attributes.depth = webAttributes.depth;
    attributes.stencil = webAttributes.stencil;
    attributes.antialias = webAttributes.antialias;
    attributes.premultipliedAlpha = webAttributes.premultipliedAlpha;
    attributes.preserveDrawingBuffer = m_private->preserveDrawingBuffer();
    attributes.preferDiscreteGPU = webAttributes.preferDiscreteGPU;
    return attributes;
}

DELEGATE_TO_WEBCONTEXT_R(getError, GC3Denum)
DELEGATE_TO_WEBCONTEXT_2(getFloatv, GC3Denum, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(getFramebufferAttachmentParameteriv, GC3Denum, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_2(getIntegerv, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(getProgramiv, Platform3DObject, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_1R(getProgramInfoLog, Platform3DObject, String)
DELEGATE_TO_WEBCONTEXT_3(getRenderbufferParameteriv, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(getShaderiv, Platform3DObject, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_1R(getShaderInfoLog, Platform3DObject, String)
DELEGATE_TO_WEBCONTEXT_4(getShaderPrecisionFormat, GC3Denum, GC3Denum, GC3Dint*, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_1R(getShaderSource, Platform3DObject, String)
DELEGATE_TO_WEBCONTEXT_1R(getString, GC3Denum, String)
DELEGATE_TO_WEBCONTEXT_3(getTexParameterfv, GC3Denum, GC3Denum, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(getTexParameteriv, GC3Denum, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(getUniformfv, Platform3DObject, GC3Dint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(getUniformiv, Platform3DObject, GC3Dint, GC3Dint*)

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    return m_private->webContext()->getUniformLocation(program, name.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_3(getVertexAttribfv, GC3Duint, GC3Denum, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(getVertexAttribiv, GC3Duint, GC3Denum, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_2R(getVertexAttribOffset, GC3Duint, GC3Denum, GC3Dsizeiptr)

DELEGATE_TO_WEBCONTEXT_2(hint, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_1R(isBuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isEnabled, GC3Denum, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isFramebuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isProgram, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isRenderbuffer, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isShader, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1R(isTexture, Platform3DObject, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_1(lineWidth, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_1(linkProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_2(pixelStorei, GC3Denum, GC3Dint)
DELEGATE_TO_WEBCONTEXT_2(polygonOffset, GC3Dfloat, GC3Dfloat)

DELEGATE_TO_WEBCONTEXT_7(readPixels, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, void*)

DELEGATE_TO_WEBCONTEXT(releaseShaderCompiler)
DELEGATE_TO_WEBCONTEXT_4(renderbufferStorage, GC3Denum, GC3Denum, GC3Dsizei, GC3Dsizei)
DELEGATE_TO_WEBCONTEXT_2(sampleCoverage, GC3Dclampf, GC3Dboolean)
DELEGATE_TO_WEBCONTEXT_4(scissor, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& string)
{
    m_private->webContext()->shaderSource(shader, string.utf8().data());
}

DELEGATE_TO_WEBCONTEXT_3(stencilFunc, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_WEBCONTEXT_4(stencilFuncSeparate, GC3Denum, GC3Denum, GC3Dint, GC3Duint)
DELEGATE_TO_WEBCONTEXT_1(stencilMask, GC3Duint)
DELEGATE_TO_WEBCONTEXT_2(stencilMaskSeparate, GC3Denum, GC3Duint)
DELEGATE_TO_WEBCONTEXT_3(stencilOp, GC3Denum, GC3Denum, GC3Denum)
DELEGATE_TO_WEBCONTEXT_4(stencilOpSeparate, GC3Denum, GC3Denum, GC3Denum, GC3Denum)

DELEGATE_TO_WEBCONTEXT_9(texImage2D, GC3Denum, GC3Dint, GC3Denum, GC3Dsizei, GC3Dsizei, GC3Dint, GC3Denum, GC3Denum, const void*)
DELEGATE_TO_WEBCONTEXT_3(texParameterf, GC3Denum, GC3Denum, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(texParameteri, GC3Denum, GC3Denum, GC3Dint)
DELEGATE_TO_WEBCONTEXT_9(texSubImage2D, GC3Denum, GC3Dint, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, const void*)

DELEGATE_TO_WEBCONTEXT_2(uniform1f, GC3Dint, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform1fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_2(uniform1i, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform1iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_3(uniform2f, GC3Dint, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform2fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(uniform2i, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform2iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_4(uniform3f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform3fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(uniform3i, GC3Dint, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform3iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_5(uniform4f, GC3Dint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_3(uniform4fv, GC3Dint, GC3Dsizei, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_5(uniform4i, GC3Dint, GC3Dint, GC3Dint, GC3Dint, GC3Dint)
DELEGATE_TO_WEBCONTEXT_3(uniform4iv, GC3Dint, GC3Dsizei, GC3Dint*)
DELEGATE_TO_WEBCONTEXT_4(uniformMatrix2fv, GC3Dint, GC3Dsizei, GC3Dboolean, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(uniformMatrix3fv, GC3Dint, GC3Dsizei, GC3Dboolean, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(uniformMatrix4fv, GC3Dint, GC3Dsizei, GC3Dboolean, GC3Dfloat*)

DELEGATE_TO_WEBCONTEXT_1(useProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(validateProgram, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_2(vertexAttrib1f, GC3Duint, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib1fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_3(vertexAttrib2f, GC3Duint, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib2fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_4(vertexAttrib3f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib3fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_5(vertexAttrib4f, GC3Duint, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat)
DELEGATE_TO_WEBCONTEXT_2(vertexAttrib4fv, GC3Duint, GC3Dfloat*)
DELEGATE_TO_WEBCONTEXT_6(vertexAttribPointer, GC3Duint, GC3Dint, GC3Denum, GC3Dboolean, GC3Dsizei, GC3Dintptr)

DELEGATE_TO_WEBCONTEXT_4(viewport, GC3Dint, GC3Dint, GC3Dsizei, GC3Dsizei)

void GraphicsContext3D::reshape(int width, int height)
{
    if (width == m_private->webContext()->width() && height == m_private->webContext()->height())
        return;

    m_private->webContext()->reshape(width, height);
}

void GraphicsContext3D::markContextChanged()
{
    m_private->markContextChanged();
}

bool GraphicsContext3D::layerComposited() const
{
    return m_private->layerComposited();
}

void GraphicsContext3D::markLayerComposited()
{
    m_private->markLayerComposited();
}

void GraphicsContext3D::paintRenderingResultsToCanvas(ImageBuffer* imageBuffer, DrawingBuffer* drawingBuffer)
{
    Platform3DObject framebufferId;
    int width, height;
    getDrawingParameters(drawingBuffer, m_private->webContext(), &framebufferId, &width, &height);
    m_private->paintFramebufferToCanvas(framebufferId, width, height, !getContextAttributes().premultipliedAlpha, imageBuffer);
}

PassRefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData(DrawingBuffer* drawingBuffer)
{
    if (getContextAttributes().premultipliedAlpha)
        return 0;

    Platform3DObject framebufferId;
    int width, height;
    getDrawingParameters(drawingBuffer, m_private->webContext(), &framebufferId, &width, &height);

    RefPtr<ImageData> imageData = ImageData::create(IntSize(width, height));
    unsigned char* pixels = imageData->data()->data();
    size_t bufferSize = 4 * width * height;

    m_private->webContext()->readBackFramebuffer(pixels, bufferSize, framebufferId, width, height);

#if (SK_R32_SHIFT == 16) && !SK_B32_SHIFT
    // If the implementation swapped the red and blue channels, un-swap them.
    for (size_t i = 0; i < bufferSize; i += 4)
        std::swap(pixels[i], pixels[i + 2]);
#endif

    return imageData.release();
}

bool GraphicsContext3D::paintCompositedResultsToCanvas(ImageBuffer*)
{
    return false;
}

DELEGATE_TO_WEBCONTEXT_R(createBuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createFramebuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createRenderbuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1R(createShader, GC3Denum, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_R(createTexture, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_1(deleteBuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteFramebuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteProgram, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteRenderbuffer, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteShader, Platform3DObject)
DELEGATE_TO_WEBCONTEXT_1(deleteTexture, Platform3DObject)

DELEGATE_TO_WEBCONTEXT_1(synthesizeGLError, GC3Denum)

Extensions3D* GraphicsContext3D::getExtensions()
{
    return m_private->getExtensions();
}

IntSize GraphicsContext3D::getInternalFramebufferSize() const
{
    return IntSize(m_private->webContext()->width(), m_private->webContext()->height());
}

bool GraphicsContext3D::texImage2DResourceSafe(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint unpackAlignment)
{
    ASSERT(unpackAlignment == 1 || unpackAlignment == 2 || unpackAlignment == 4 || unpackAlignment == 8);
    OwnArrayPtr<unsigned char> zero;
    if (!isResourceSafe() && width > 0 && height > 0) {
        unsigned int size;
        GC3Denum error = computeImageSizeInBytes(format, type, width, height, unpackAlignment, &size, 0);
        if (error != GraphicsContext3D::NO_ERROR) {
            synthesizeGLError(error);
            return false;
        }
        zero = adoptArrayPtr(new unsigned char[size]);
        if (!zero) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return false;
        }
        memset(zero.get(), 0, size);
    }
    texImage2D(target, level, internalformat, width, height, border, format, type, zero.get());
    return true;
}

bool GraphicsContext3D::computeFormatAndTypeParameters(GC3Denum format,
                                                       GC3Denum type,
                                                       unsigned int* componentsPerPixel,
                                                       unsigned int* bytesPerComponent)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::DEPTH_COMPONENT:
    case GraphicsContext3D::DEPTH_STENCIL:
        *componentsPerPixel = 1;
        break;
    case GraphicsContext3D::LUMINANCE_ALPHA:
        *componentsPerPixel = 2;
        break;
    case GraphicsContext3D::RGB:
        *componentsPerPixel = 3;
        break;
    case GraphicsContext3D::RGBA:
    case Extensions3D::BGRA_EXT: // GL_EXT_texture_format_BGRA8888
        *componentsPerPixel = 4;
        break;
    default:
        return false;
    }
    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
        *bytesPerComponent = sizeof(GC3Dubyte);
        break;
    case GraphicsContext3D::UNSIGNED_SHORT:
        *bytesPerComponent = sizeof(GC3Dushort);
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(GC3Dushort);
        break;
    case GraphicsContext3D::UNSIGNED_INT_24_8:
    case GraphicsContext3D::UNSIGNED_INT:
        *bytesPerComponent = sizeof(GC3Duint);
        break;
    case GraphicsContext3D::FLOAT: // OES_texture_float
        *bytesPerComponent = sizeof(GC3Dfloat);
        break;
    case GraphicsContext3D::HALF_FLOAT_OES: // OES_texture_half_float
        *bytesPerComponent = sizeof(GC3Dhalffloat);
        break;
    default:
        return false;
    }
    return true;
}

GC3Denum GraphicsContext3D::computeImageSizeInBytes(GC3Denum format, GC3Denum type, GC3Dsizei width, GC3Dsizei height, GC3Dint alignment,
                                                    unsigned int* imageSizeInBytes, unsigned int* paddingInBytes)
{
    ASSERT(imageSizeInBytes);
    ASSERT(alignment == 1 || alignment == 2 || alignment == 4 || alignment == 8);
    if (width < 0 || height < 0)
        return GraphicsContext3D::INVALID_VALUE;
    unsigned int bytesPerComponent, componentsPerPixel;
    if (!computeFormatAndTypeParameters(format, type, &bytesPerComponent, &componentsPerPixel))
        return GraphicsContext3D::INVALID_ENUM;
    if (!width || !height) {
        *imageSizeInBytes = 0;
        if (paddingInBytes)
            *paddingInBytes = 0;
        return GraphicsContext3D::NO_ERROR;
    }
    CheckedInt<uint32_t> checkedValue(bytesPerComponent * componentsPerPixel);
    checkedValue *=  width;
    if (!checkedValue.isValid())
        return GraphicsContext3D::INVALID_VALUE;
    unsigned int validRowSize = checkedValue.value();
    unsigned int padding = 0;
    unsigned int residual = validRowSize % alignment;
    if (residual) {
        padding = alignment - residual;
        checkedValue += padding;
    }
    // Last row needs no padding.
    checkedValue *= (height - 1);
    checkedValue += validRowSize;
    if (!checkedValue.isValid())
        return GraphicsContext3D::INVALID_VALUE;
    *imageSizeInBytes = checkedValue.value();
    if (paddingInBytes)
        *paddingInBytes = padding;
    return GraphicsContext3D::NO_ERROR;
}

GraphicsContext3D::ImageExtractor::ImageExtractor(Image* image, ImageHtmlDomSource imageHtmlDomSource, bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
{
    m_image = image;
    m_imageHtmlDomSource = imageHtmlDomSource;
    m_extractSucceeded = extractImage(premultiplyAlpha, ignoreGammaAndColorProfile);
}

GraphicsContext3D::ImageExtractor::~ImageExtractor()
{
    if (m_skiaImage)
        m_skiaImage->bitmap().unlockPixels();
}

bool GraphicsContext3D::ImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
{
    if (!m_image)
        return false;
    m_skiaImage = m_image->nativeImageForCurrentFrame();
    m_alphaOp = AlphaDoNothing;
    bool hasAlpha = m_skiaImage ? !m_skiaImage->bitmap().isOpaque() : true;
    if ((!m_skiaImage || ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && m_image->data()) {
        // Attempt to get raw unpremultiplied image data.
        OwnPtr<ImageDecoder> decoder(ImageDecoder::create(
            *(m_image->data()), ImageSource::AlphaNotPremultiplied,
            ignoreGammaAndColorProfile ? ImageSource::GammaAndColorProfileIgnored : ImageSource::GammaAndColorProfileApplied));
        if (!decoder)
            return false;
        decoder->setData(m_image->data(), true);
        if (!decoder->frameCount())
            return false;
        ImageFrame* frame = decoder->frameBufferAtIndex(0);
        if (!frame || frame->status() != ImageFrame::FrameComplete)
            return false;
        hasAlpha = frame->hasAlpha();
        m_nativeImage = frame->asNewNativeImage();
        if (!m_nativeImage.get() || !m_nativeImage->isDataComplete() || !m_nativeImage->bitmap().width() || !m_nativeImage->bitmap().height())
            return false;
        SkBitmap::Config skiaConfig = m_nativeImage->bitmap().config();
        if (skiaConfig != SkBitmap::kARGB_8888_Config)
            return false;
        m_skiaImage = m_nativeImage.get();
        if (hasAlpha && premultiplyAlpha)
            m_alphaOp = AlphaDoPremultiply;
    } else if (!premultiplyAlpha && hasAlpha) {
        // 1. For texImage2D with HTMLVideoElment input, assume no PremultiplyAlpha had been applied and the alpha value for each pixel is 0xFF
        // which is true at present and may be changed in the future and needs adjustment accordingly.
        // 2. For texImage2D with HTMLCanvasElement input in which Alpha is already Premultiplied in this port,
        // do AlphaDoUnmultiply if UNPACK_PREMULTIPLY_ALPHA_WEBGL is set to false.
        if (m_imageHtmlDomSource != HtmlDomVideo)
            m_alphaOp = AlphaDoUnmultiply;
    }
    if (!m_skiaImage)
        return false;

    m_imageSourceFormat = SK_B32_SHIFT ? DataFormatRGBA8 : DataFormatBGRA8;
    m_imageWidth = m_skiaImage->bitmap().width();
    m_imageHeight = m_skiaImage->bitmap().height();
    if (!m_imageWidth || !m_imageHeight)
        return false;
    m_imageSourceUnpackAlignment = 0;
    m_skiaImage->bitmap().lockPixels();
    m_imagePixelData = m_skiaImage->bitmap().getPixels();
    return true;
}

bool GraphicsContext3D::packImageData(
    Image* image,
    const void* pixels,
    GC3Denum format,
    GC3Denum type,
    bool flipY,
    AlphaOp alphaOp,
    DataFormat sourceFormat,
    unsigned width,
    unsigned height,
    unsigned sourceUnpackAlignment,
    Vector<uint8_t>& data)
{
    if (!pixels)
        return false;

    unsigned packedSize;
    // Output data is tightly packed (alignment == 1).
    if (computeImageSizeInBytes(format, type, width, height, 1, &packedSize, 0) != GraphicsContext3D::NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(reinterpret_cast<const uint8_t*>(pixels), sourceFormat, width, height, sourceUnpackAlignment, format, type, alphaOp, data.data(), flipY))
        return false;
    if (ImageObserver *observer = image->imageObserver())
        observer->didDraw(image);
    return true;
}

bool GraphicsContext3D::extractImageData(ImageData* imageData,
                                         GC3Denum format,
                                         GC3Denum type,
                                         bool flipY,
                                         bool premultiplyAlpha,
                                         Vector<uint8_t>& data)
{
    if (!imageData)
        return false;
    int width = imageData->width();
    int height = imageData->height();

    unsigned int packedSize;
    // Output data is tightly packed (alignment == 1).
    if (computeImageSizeInBytes(format, type, width, height, 1, &packedSize, 0) != GraphicsContext3D::NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(imageData->data()->data(), DataFormatRGBA8, width, height, 0, format, type, premultiplyAlpha ? AlphaDoPremultiply : AlphaDoNothing, data.data(), flipY))
        return false;

    return true;
}

bool GraphicsContext3D::extractTextureData(unsigned int width, unsigned int height,
                                           GC3Denum format, GC3Denum type,
                                           unsigned int unpackAlignment,
                                           bool flipY, bool premultiplyAlpha,
                                           const void* pixels,
                                           Vector<uint8_t>& data)
{
    // Assumes format, type, etc. have already been validated.
    DataFormat sourceDataFormat = getDataFormat(format, type);

    // Resize the output buffer.
    unsigned int componentsPerPixel, bytesPerComponent;
    if (!computeFormatAndTypeParameters(format, type,
                                        &componentsPerPixel,
                                        &bytesPerComponent))
        return false;
    unsigned int bytesPerPixel = componentsPerPixel * bytesPerComponent;
    data.resize(width * height * bytesPerPixel);

    if (!packPixels(static_cast<const uint8_t*>(pixels), sourceDataFormat, width, height, unpackAlignment, format, type, (premultiplyAlpha ? AlphaDoPremultiply : AlphaDoNothing), data.data(), flipY))
        return false;

    return true;
}

namespace {

/* BEGIN CODE SHARED WITH MOZILLA FIREFOX */

// The following packing and unpacking routines are expressed in terms of function templates and inline functions to achieve generality and speedup.
// Explicit template specializations correspond to the cases that would occur.
// Some code are merged back from Mozilla code in http://mxr.mozilla.org/mozilla-central/source/content/canvas/src/WebGLTexelConversions.h

//----------------------------------------------------------------------
// Pixel unpacking routines.
template<int format, typename SourceType, typename DstType>
ALWAYS_INLINE void unpack(const SourceType*, DstType*, unsigned)
{
    ASSERT_NOT_REACHED();
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatRGB8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        destination[3] = 0xFF;
        source += 3;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatBGR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[2];
        destination[1] = source[1];
        destination[2] = source[0];
        destination[3] = 0xFF;
        source += 3;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatARGB8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[1];
        destination[1] = source[2];
        destination[2] = source[3];
        destination[3] = source[0];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatABGR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3];
        destination[1] = source[2];
        destination[2] = source[1];
        destination[3] = source[0];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatBGRA8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    const uint32_t* source32 = reinterpret_cast_ptr<const uint32_t*>(source);
    uint32_t* destination32 = reinterpret_cast_ptr<uint32_t*>(destination);
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint32_t bgra = source32[i];
#if CPU(BIG_ENDIAN)
        uint32_t brMask = 0xff00ff00;
        uint32_t gaMask = 0x00ff00ff;
#else
        uint32_t brMask = 0x00ff00ff;
        uint32_t gaMask = 0xff00ff00;
#endif
        uint32_t rgba = (((bgra >> 16) | (bgra << 16)) & brMask) | (bgra & gaMask);
        destination32[i] = rgba;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatRGBA5551, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGBA5551ToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint16_t packedValue = source[0];
        uint8_t r = packedValue >> 11;
        uint8_t g = (packedValue >> 6) & 0x1F;
        uint8_t b = (packedValue >> 1) & 0x1F;
        destination[0] = (r << 3) | (r & 0x7);
        destination[1] = (g << 3) | (g & 0x7);
        destination[2] = (b << 3) | (b & 0x7);
        destination[3] = (packedValue & 0x1) ? 0xFF : 0x0;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatRGBA4444, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGBA4444ToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint16_t packedValue = source[0];
        uint8_t r = packedValue >> 12;
        uint8_t g = (packedValue >> 8) & 0x0F;
        uint8_t b = (packedValue >> 4) & 0x0F;
        uint8_t a = packedValue & 0x0F;
        destination[0] = r << 4 | r;
        destination[1] = g << 4 | g;
        destination[2] = b << 4 | b;
        destination[3] = a << 4 | a;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatRGB565, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGB565ToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint16_t packedValue = source[0];
        uint8_t r = packedValue >> 11;
        uint8_t g = (packedValue >> 5) & 0x3F;
        uint8_t b = packedValue & 0x1F;
        destination[0] = (r << 3) | (r & 0x7);
        destination[1] = (g << 2) | (g & 0x3);
        destination[2] = (b << 3) | (b & 0x7);
        destination[3] = 0xFF;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = 0xFF;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatRA8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = source[1];
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatAR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[1];
        destination[1] = source[1];
        destination[2] = source[1];
        destination[3] = source[0];
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatA8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0x0;
        destination[1] = 0x0;
        destination[2] = 0x0;
        destination[3] = source[0];
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatRGBA8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = source[3] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatBGRA8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[2] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[0] * scaleFactor;
        destination[3] = source[3] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatABGR8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3] * scaleFactor;
        destination[1] = source[2] * scaleFactor;
        destination[2] = source[1] * scaleFactor;
        destination[3] = source[0] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatARGB8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[1] * scaleFactor;
        destination[1] = source[2] * scaleFactor;
        destination[2] = source[3] * scaleFactor;
        destination[3] = source[0] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatRGB8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = 1;
        source += 3;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatBGR8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[2] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[0] * scaleFactor;
        destination[3] = 1;
        source += 3;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatRGB32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        destination[3] = 1;
        source += 3;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatR32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = 1;
        source += 1;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatRA32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = source[1];
        source += 2;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void unpack<GraphicsContext3D::DataFormatA32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0;
        destination[1] = 0;
        destination[2] = 0;
        destination[3] = source[0];
        source += 1;
        destination += 4;
    }
}

//----------------------------------------------------------------------
// Pixel packing routines.
//

template<int format, int alphaOp, typename SourceType, typename DstType>
ALWAYS_INLINE void pack(const SourceType*, DstType*, unsigned)
{
    ASSERT_NOT_REACHED();
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatA8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3];
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatR8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatR8, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatR8, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRA8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRA8, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRA8, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB8, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        source += 4;
        destination += 3;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB8, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        source += 4;
        destination += 3;
    }
}


template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    memcpy(destination, source, pixelsPerRow * 4);
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA8, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA8, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA4444, GraphicsContext3D::AlphaDoNothing, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::packOneRowOfRGBA8ToUnsignedShort4444(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        *destination = (((source[0] & 0xF0) << 8)
                        | ((source[1] & 0xF0) << 4)
                        | (source[2] & 0xF0)
                        | (source[3] >> 4));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA4444, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF0) << 8)
                        | ((sourceG & 0xF0) << 4)
                        | (sourceB & 0xF0)
                        | (source[3] >> 4));
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA4444, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF0) << 8)
                        | ((sourceG & 0xF0) << 4)
                        | (sourceB & 0xF0)
                        | (source[3] >> 4));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA5551, GraphicsContext3D::AlphaDoNothing, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::packOneRowOfRGBA8ToUnsignedShort5551(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        *destination = (((source[0] & 0xF8) << 8)
                        | ((source[1] & 0xF8) << 3)
                        | ((source[2] & 0xF8) >> 2)
                        | (source[3] >> 7));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA5551, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xF8) << 3)
                        | ((sourceB & 0xF8) >> 2)
                        | (source[3] >> 7));
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA5551, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xF8) << 3)
                        | ((sourceB & 0xF8) >> 2)
                        | (source[3] >> 7));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB565, GraphicsContext3D::AlphaDoNothing, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::packOneRowOfRGBA8ToUnsignedShort565(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        *destination = (((source[0] & 0xF8) << 8)
                        | ((source[1] & 0xFC) << 3)
                        | ((source[2] & 0xF8) >> 3));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB565, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xFC) << 3)
                        | ((sourceB & 0xF8) >> 3));
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB565, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xFC) << 3)
                        | ((sourceB & 0xF8) >> 3));
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB32F, GraphicsContext3D::AlphaDoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB32F, GraphicsContext3D::AlphaDoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        source += 4;
        destination += 3;
    }
}

// Used only during RGBA8 or BGRA8 -> floating-point uploads.
template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    memcpy(destination, source, pixelsPerRow * 4 * sizeof(float));
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA32F, GraphicsContext3D::AlphaDoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA32F, GraphicsContext3D::AlphaDoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatA32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3];
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatR32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatR32F, GraphicsContext3D::AlphaDoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatR32F, GraphicsContext3D::AlphaDoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRA32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRA32F, GraphicsContext3D::AlphaDoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRA32F, GraphicsContext3D::AlphaDoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[1]);
        destination[2] = convertFloatToHalfFloat(source[2]);
        destination[3] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA16F, GraphicsContext3D::AlphaDoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        destination[3] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGBA16F, GraphicsContext3D::AlphaDoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        destination[3] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[1]);
        destination[2] = convertFloatToHalfFloat(source[2]);
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB16F, GraphicsContext3D::AlphaDoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRGB16F, GraphicsContext3D::AlphaDoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        source += 4;
        destination += 3;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRA16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRA16F, GraphicsContext3D::AlphaDoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatRA16F, GraphicsContext3D::AlphaDoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 2;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatR16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatR16F, GraphicsContext3D::AlphaDoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatR16F, GraphicsContext3D::AlphaDoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        source += 4;
        destination += 1;
    }
}

template<> ALWAYS_INLINE void pack<GraphicsContext3D::DataFormatA16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 1;
    }
}

ALWAYS_INLINE bool HasAlpha(int format)
{
    return format == GraphicsContext3D::DataFormatA8
        || format == GraphicsContext3D::DataFormatA16F
        || format == GraphicsContext3D::DataFormatA32F
        || format == GraphicsContext3D::DataFormatRA8
        || format == GraphicsContext3D::DataFormatAR8
        || format == GraphicsContext3D::DataFormatRA16F
        || format == GraphicsContext3D::DataFormatRA32F
        || format == GraphicsContext3D::DataFormatRGBA8
        || format == GraphicsContext3D::DataFormatBGRA8
        || format == GraphicsContext3D::DataFormatARGB8
        || format == GraphicsContext3D::DataFormatABGR8
        || format == GraphicsContext3D::DataFormatRGBA16F
        || format == GraphicsContext3D::DataFormatRGBA32F
        || format == GraphicsContext3D::DataFormatRGBA4444
        || format == GraphicsContext3D::DataFormatRGBA5551;
}

ALWAYS_INLINE bool HasColor(int format)
{
    return format == GraphicsContext3D::DataFormatRGBA8
        || format == GraphicsContext3D::DataFormatRGBA16F
        || format == GraphicsContext3D::DataFormatRGBA32F
        || format == GraphicsContext3D::DataFormatRGB8
        || format == GraphicsContext3D::DataFormatRGB16F
        || format == GraphicsContext3D::DataFormatRGB32F
        || format == GraphicsContext3D::DataFormatBGR8
        || format == GraphicsContext3D::DataFormatBGRA8
        || format == GraphicsContext3D::DataFormatARGB8
        || format == GraphicsContext3D::DataFormatABGR8
        || format == GraphicsContext3D::DataFormatRGBA5551
        || format == GraphicsContext3D::DataFormatRGBA4444
        || format == GraphicsContext3D::DataFormatRGB565
        || format == GraphicsContext3D::DataFormatR8
        || format == GraphicsContext3D::DataFormatR16F
        || format == GraphicsContext3D::DataFormatR32F
        || format == GraphicsContext3D::DataFormatRA8
        || format == GraphicsContext3D::DataFormatRA16F
        || format == GraphicsContext3D::DataFormatRA32F
        || format == GraphicsContext3D::DataFormatAR8;
}

template<int Format>
struct IsFloatFormat {
    static const bool Value =
        Format == GraphicsContext3D::DataFormatRGBA32F
        || Format == GraphicsContext3D::DataFormatRGB32F
        || Format == GraphicsContext3D::DataFormatRA32F
        || Format == GraphicsContext3D::DataFormatR32F
        || Format == GraphicsContext3D::DataFormatA32F;
};

template<int Format>
struct IsHalfFloatFormat {
    static const bool Value =
        Format == GraphicsContext3D::DataFormatRGBA16F
        || Format == GraphicsContext3D::DataFormatRGB16F
        || Format == GraphicsContext3D::DataFormatRA16F
        || Format == GraphicsContext3D::DataFormatR16F
        || Format == GraphicsContext3D::DataFormatA16F;
};

template<int Format>
struct Is16bppFormat {
    static const bool Value =
        Format == GraphicsContext3D::DataFormatRGBA5551
        || Format == GraphicsContext3D::DataFormatRGBA4444
        || Format == GraphicsContext3D::DataFormatRGB565;
};

template<int Format, bool IsFloat = IsFloatFormat<Format>::Value, bool IsHalfFloat = IsHalfFloatFormat<Format>::Value, bool Is16bpp = Is16bppFormat<Format>::Value>
struct DataTypeForFormat {
    typedef uint8_t Type;
};

template<int Format>
struct DataTypeForFormat<Format, true, false, false> {
    typedef float Type;
};

template<int Format>
struct DataTypeForFormat<Format, false, true, false> {
    typedef uint16_t Type;
};

template<int Format>
struct DataTypeForFormat<Format, false, false, true> {
    typedef uint16_t Type;
};

template<int Format>
struct IntermediateFormat {
    static const int Value = (IsFloatFormat<Format>::Value || IsHalfFloatFormat<Format>::Value) ? GraphicsContext3D::DataFormatRGBA32F : GraphicsContext3D::DataFormatRGBA8;
};

ALWAYS_INLINE unsigned TexelBytesForFormat(GraphicsContext3D::DataFormat format)
{
    switch (format) {
    case GraphicsContext3D::DataFormatR8:
    case GraphicsContext3D::DataFormatA8:
        return 1;
    case GraphicsContext3D::DataFormatRA8:
    case GraphicsContext3D::DataFormatAR8:
    case GraphicsContext3D::DataFormatRGBA5551:
    case GraphicsContext3D::DataFormatRGBA4444:
    case GraphicsContext3D::DataFormatRGB565:
    case GraphicsContext3D::DataFormatA16F:
    case GraphicsContext3D::DataFormatR16F:
        return 2;
    case GraphicsContext3D::DataFormatRGB8:
    case GraphicsContext3D::DataFormatBGR8:
        return 3;
    case GraphicsContext3D::DataFormatRGBA8:
    case GraphicsContext3D::DataFormatARGB8:
    case GraphicsContext3D::DataFormatABGR8:
    case GraphicsContext3D::DataFormatBGRA8:
    case GraphicsContext3D::DataFormatR32F:
    case GraphicsContext3D::DataFormatA32F:
    case GraphicsContext3D::DataFormatRA16F:
        return 4;
    case GraphicsContext3D::DataFormatRGB16F:
        return 6;
    case GraphicsContext3D::DataFormatRA32F:
    case GraphicsContext3D::DataFormatRGBA16F:
        return 8;
    case GraphicsContext3D::DataFormatRGB32F:
        return 12;
    case GraphicsContext3D::DataFormatRGBA32F:
        return 16;
    default:
        return 0;
    }
}

/* END CODE SHARED WITH MOZILLA FIREFOX */

class FormatConverter {
public:
    FormatConverter(unsigned width, unsigned height,
        const void* srcStart, void* dstStart, int srcStride, int dstStride)
        : m_width(width), m_height(height), m_srcStart(srcStart), m_dstStart(dstStart), m_srcStride(srcStride), m_dstStride(dstStride), m_success(false)
    {
        const unsigned MaxNumberOfComponents = 4;
        const unsigned MaxBytesPerComponent  = 4;
        m_unpackedIntermediateSrcData = adoptArrayPtr(new uint8_t[m_width * MaxNumberOfComponents *MaxBytesPerComponent]);
        ASSERT(m_unpackedIntermediateSrcData.get());
    }

    void convert(GraphicsContext3D::DataFormat srcFormat, GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp);
    bool Success() const { return m_success; }

private:
    template<GraphicsContext3D::DataFormat SrcFormat>
    ALWAYS_INLINE void convert(GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp);

    template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat>
    ALWAYS_INLINE void convert(GraphicsContext3D::AlphaOp);

    template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat, GraphicsContext3D::AlphaOp alphaOp>
    ALWAYS_INLINE void convert();

    const unsigned m_width, m_height;
    const void* const m_srcStart;
    void* const m_dstStart;
    const int m_srcStride, m_dstStride;
    bool m_success;
    OwnArrayPtr<uint8_t> m_unpackedIntermediateSrcData;
};

void FormatConverter::convert(GraphicsContext3D::DataFormat srcFormat, GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp alphaOp)
{
#define FORMATCONVERTER_CASE_SRCFORMAT(SrcFormat) \
    case SrcFormat: \
        return convert<SrcFormat>(dstFormat, alphaOp);

        switch (srcFormat) {
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatR32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatA32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRA32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGB8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatBGR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGB565)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGB32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGBA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatARGB8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatABGR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatAR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatBGRA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGBA5551)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGBA4444)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGBA32F)
        default:
            ASSERT_NOT_REACHED();
        }
#undef FORMATCONVERTER_CASE_SRCFORMAT
}

template<GraphicsContext3D::DataFormat SrcFormat>
ALWAYS_INLINE void FormatConverter::convert(GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp alphaOp)
{
#define FORMATCONVERTER_CASE_DSTFORMAT(DstFormat) \
    case DstFormat: \
        return convert<SrcFormat, DstFormat>(alphaOp);

        switch (dstFormat) {
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatR8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatR16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatR32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatA8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatA16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatA32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRA8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRA16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRA32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGB8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGB565)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGB16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGB32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA5551)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA4444)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA32F)
        default:
            ASSERT_NOT_REACHED();
        }

#undef FORMATCONVERTER_CASE_DSTFORMAT
}

template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat>
ALWAYS_INLINE void FormatConverter::convert(GraphicsContext3D::AlphaOp alphaOp)
{
#define FORMATCONVERTER_CASE_ALPHAOP(alphaOp) \
    case alphaOp: \
        return convert<SrcFormat, DstFormat, alphaOp>();

        switch (alphaOp) {
            FORMATCONVERTER_CASE_ALPHAOP(GraphicsContext3D::AlphaDoNothing)
            FORMATCONVERTER_CASE_ALPHAOP(GraphicsContext3D::AlphaDoPremultiply)
            FORMATCONVERTER_CASE_ALPHAOP(GraphicsContext3D::AlphaDoUnmultiply)
        default:
            ASSERT_NOT_REACHED();
        }
#undef FORMATCONVERTER_CASE_ALPHAOP
}

template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat, GraphicsContext3D::AlphaOp alphaOp>
ALWAYS_INLINE void FormatConverter::convert()
{
    // Many instantiations of this template function will never be entered, so we try
    // to return immediately in these cases to avoid the compiler to generate useless code.
    if (SrcFormat == DstFormat && alphaOp == GraphicsContext3D::AlphaDoNothing) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!IsFloatFormat<DstFormat>::Value && IsFloatFormat<SrcFormat>::Value) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Only textures uploaded from DOM elements or ImageData can allow DstFormat != SrcFormat.
    const bool srcFormatComesFromDOMElementOrImageData = GraphicsContext3D::srcFormatComeFromDOMElementOrImageData(SrcFormat);
    if (!srcFormatComesFromDOMElementOrImageData && SrcFormat != DstFormat) {
        ASSERT_NOT_REACHED();
        return;
    }
    // Likewise, only textures uploaded from DOM elements or ImageData can possibly have to be unpremultiplied.
    if (!srcFormatComesFromDOMElementOrImageData && alphaOp == GraphicsContext3D::AlphaDoUnmultiply) {
        ASSERT_NOT_REACHED();
        return;
    }
    if ((!HasAlpha(SrcFormat) || !HasColor(SrcFormat) || !HasColor(DstFormat)) && alphaOp != GraphicsContext3D::AlphaDoNothing) {
        ASSERT_NOT_REACHED();
        return;
    }

    typedef typename DataTypeForFormat<SrcFormat>::Type SrcType;
    typedef typename DataTypeForFormat<DstFormat>::Type DstType;
    const int IntermediateSrcFormat = IntermediateFormat<DstFormat>::Value;
    typedef typename DataTypeForFormat<IntermediateSrcFormat>::Type IntermediateSrcType;
    const ptrdiff_t srcStrideInElements = m_srcStride / sizeof(SrcType);
    const ptrdiff_t dstStrideInElements = m_dstStride / sizeof(DstType);
    const bool trivialUnpack = (SrcFormat == GraphicsContext3D::DataFormatRGBA8 && !IsFloatFormat<DstFormat>::Value && !IsHalfFloatFormat<DstFormat>::Value) || SrcFormat == GraphicsContext3D::DataFormatRGBA32F;
    const bool trivialPack = (DstFormat == GraphicsContext3D::DataFormatRGBA8 || DstFormat == GraphicsContext3D::DataFormatRGBA32F) && alphaOp == GraphicsContext3D::AlphaDoNothing && m_dstStride > 0;
    ASSERT(!trivialUnpack || !trivialPack);

    const SrcType *srcRowStart = static_cast<const SrcType*>(m_srcStart);
    DstType* dstRowStart = static_cast<DstType*>(m_dstStart);
    if (!trivialUnpack && trivialPack) {
        for (size_t i = 0; i < m_height; ++i) {
            unpack<SrcFormat>(srcRowStart, dstRowStart, m_width);
            srcRowStart += srcStrideInElements;
            dstRowStart += dstStrideInElements;
        }
    } else if (!trivialUnpack && !trivialPack) {
        for (size_t i = 0; i < m_height; ++i) {
            unpack<SrcFormat>(srcRowStart, reinterpret_cast<IntermediateSrcType*>(m_unpackedIntermediateSrcData.get()), m_width);
            pack<DstFormat, alphaOp>(reinterpret_cast<IntermediateSrcType*>(m_unpackedIntermediateSrcData.get()), dstRowStart, m_width);
            srcRowStart += srcStrideInElements;
            dstRowStart += dstStrideInElements;
        }
    } else {
        for (size_t i = 0; i < m_height; ++i) {
            pack<DstFormat, alphaOp>(srcRowStart, dstRowStart, m_width);
            srcRowStart += srcStrideInElements;
            dstRowStart += dstStrideInElements;
        }
    }
    m_success = true;
    return;
}

} // anonymous namespace

bool GraphicsContext3D::packPixels(const uint8_t* sourceData, DataFormat sourceDataFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, unsigned destinationFormat, unsigned destinationType, AlphaOp alphaOp, void* destinationData, bool flipY)
{
    int validSrc = width * TexelBytesForFormat(sourceDataFormat);
    int remainder = sourceUnpackAlignment ? (validSrc % sourceUnpackAlignment) : 0;
    int srcStride = remainder ? (validSrc + sourceUnpackAlignment - remainder) : validSrc;

    DataFormat dstDataFormat = getDataFormat(destinationFormat, destinationType);
    int dstStride = width * TexelBytesForFormat(dstDataFormat);
    if (flipY) {
        destinationData = static_cast<uint8_t*>(destinationData) + dstStride*(height - 1);
        dstStride = -dstStride;
    }
    if (!HasAlpha(sourceDataFormat) || !HasColor(sourceDataFormat) || !HasColor(dstDataFormat))
        alphaOp = AlphaDoNothing;

    if (sourceDataFormat == dstDataFormat && alphaOp == AlphaDoNothing) {
        const uint8_t* ptr = sourceData;
        const uint8_t* ptrEnd = sourceData + srcStride * height;
        unsigned rowSize = (dstStride > 0) ? dstStride: -dstStride;
        uint8_t* dst = static_cast<uint8_t*>(destinationData);
        while (ptr < ptrEnd) {
            memcpy(dst, ptr, rowSize);
            ptr += srcStride;
            dst += dstStride;
        }
        return true;
    }

    FormatConverter converter(width, height, sourceData, destinationData, srcStride, dstStride);
    converter.convert(sourceDataFormat, dstDataFormat, alphaOp);
    if (!converter.Success())
        return false;
    return true;
}

unsigned GraphicsContext3D::getClearBitsByAttachmentType(GC3Denum attachment)
{
    switch (attachment) {
    case GraphicsContext3D::COLOR_ATTACHMENT0:
    case Extensions3D::COLOR_ATTACHMENT1_EXT:
    case Extensions3D::COLOR_ATTACHMENT2_EXT:
    case Extensions3D::COLOR_ATTACHMENT3_EXT:
    case Extensions3D::COLOR_ATTACHMENT4_EXT:
    case Extensions3D::COLOR_ATTACHMENT5_EXT:
    case Extensions3D::COLOR_ATTACHMENT6_EXT:
    case Extensions3D::COLOR_ATTACHMENT7_EXT:
    case Extensions3D::COLOR_ATTACHMENT8_EXT:
    case Extensions3D::COLOR_ATTACHMENT9_EXT:
    case Extensions3D::COLOR_ATTACHMENT10_EXT:
    case Extensions3D::COLOR_ATTACHMENT11_EXT:
    case Extensions3D::COLOR_ATTACHMENT12_EXT:
    case Extensions3D::COLOR_ATTACHMENT13_EXT:
    case Extensions3D::COLOR_ATTACHMENT14_EXT:
    case Extensions3D::COLOR_ATTACHMENT15_EXT:
        return GraphicsContext3D::COLOR_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_ATTACHMENT:
        return GraphicsContext3D::DEPTH_BUFFER_BIT;
    case GraphicsContext3D::STENCIL_ATTACHMENT:
        return GraphicsContext3D::STENCIL_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_STENCIL_ATTACHMENT:
        return GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

unsigned GraphicsContext3D::getClearBitsByFormat(GC3Denum format)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::RGB565:
    case GraphicsContext3D::RGBA:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RGB5_A1:
        return GraphicsContext3D::COLOR_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::DEPTH_COMPONENT:
        return GraphicsContext3D::DEPTH_BUFFER_BIT;
    case GraphicsContext3D::STENCIL_INDEX8:
        return GraphicsContext3D::STENCIL_BUFFER_BIT;
    case GraphicsContext3D::DEPTH_STENCIL:
        return GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

unsigned GraphicsContext3D::getChannelBitsByFormat(GC3Denum format)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
        return ChannelAlpha;
    case GraphicsContext3D::LUMINANCE:
        return ChannelRGB;
    case GraphicsContext3D::LUMINANCE_ALPHA:
        return ChannelRGBA;
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::RGB565:
        return ChannelRGB;
    case GraphicsContext3D::RGBA:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RGB5_A1:
        return ChannelRGBA;
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::DEPTH_COMPONENT:
        return ChannelDepth;
    case GraphicsContext3D::STENCIL_INDEX8:
        return ChannelStencil;
    case GraphicsContext3D::DEPTH_STENCIL:
        return ChannelDepth | ChannelStencil;
    default:
        return 0;
    }
}

} // namespace WebCore
