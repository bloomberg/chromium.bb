// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/apple/fx_quartz_device.h"

#include "core/fxcrt/fx_extension.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "core/fxge/fx_freetype.h"

#if !defined _SKIA_SUPPORT_ && !defined _SKIA_SUPPORT_PATHS_
#include "core/fxge/agg/fx_agg_driver.h"
#endif

#ifndef CGFLOAT_IS_DOUBLE
#error Expected CGFLOAT_IS_DOUBLE to be defined by CoreGraphics headers
#endif

void* CQuartz2D::CreateGraphics(const RetainPtr<CFX_DIBitmap>& pBitmap) {
  if (!pBitmap)
    return nullptr;
  CGBitmapInfo bmpInfo = kCGBitmapByteOrder32Little;
  switch (pBitmap->GetFormat()) {
    case FXDIB_Rgb32:
      bmpInfo |= kCGImageAlphaNoneSkipFirst;
      break;
    case FXDIB_Argb:
    default:
      return nullptr;
  }
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      pBitmap->GetBuffer(), pBitmap->GetWidth(), pBitmap->GetHeight(), 8,
      pBitmap->GetPitch(), colorSpace, bmpInfo);
  CGColorSpaceRelease(colorSpace);
  return context;
}

void CQuartz2D::DestroyGraphics(void* graphics) {
  if (graphics)
    CGContextRelease((CGContextRef)graphics);
}

void* CQuartz2D::CreateFont(const uint8_t* pFontData, uint32_t dwFontSize) {
  CGDataProviderRef pDataProvider = CGDataProviderCreateWithData(
      nullptr, pFontData, static_cast<size_t>(dwFontSize), nullptr);
  if (!pDataProvider)
    return nullptr;

  CGFontRef pCGFont = CGFontCreateWithDataProvider(pDataProvider);
  CGDataProviderRelease(pDataProvider);
  return pCGFont;
}

void CQuartz2D::DestroyFont(void* pFont) {
  CGFontRelease((CGFontRef)pFont);
}

void CQuartz2D::SetGraphicsTextMatrix(void* graphics,
                                      const CFX_Matrix& matrix) {
  if (!graphics)
    return;
  CGContextRef context = reinterpret_cast<CGContextRef>(graphics);
  CGFloat ty = CGBitmapContextGetHeight(context) - matrix.f;
  CGContextSetTextMatrix(
      context, CGAffineTransformMake(matrix.a, matrix.b, matrix.c, matrix.d,
                                     matrix.e, ty));
}

bool CQuartz2D::DrawGraphicsString(void* graphics,
                                   void* font,
                                   float fontSize,
                                   uint16_t* glyphIndices,
                                   CGPoint* glyphPositions,
                                   int32_t charsCount,
                                   FX_ARGB argb) {
  if (!graphics)
    return false;

  CGContextRef context = (CGContextRef)graphics;
  CGContextSetFont(context, (CGFontRef)font);
  CGContextSetFontSize(context, fontSize);

  int32_t a;
  int32_t r;
  int32_t g;
  int32_t b;
  std::tie(a, r, g, b) = ArgbDecode(argb);
  CGContextSetRGBFillColor(context, r / 255.f, g / 255.f, b / 255.f, a / 255.f);
  CGContextSaveGState(context);
#if CGFLOAT_IS_DOUBLE
  CGPoint* glyphPositionsCG = new CGPoint[charsCount];
  for (int index = 0; index < charsCount; ++index) {
    glyphPositionsCG[index].x = glyphPositions[index].x;
    glyphPositionsCG[index].y = glyphPositions[index].y;
  }
#else
  CGPoint* glyphPositionsCG = glyphPositions;
#endif
  CGContextShowGlyphsAtPositions(context,
                                 reinterpret_cast<CGGlyph*>(glyphIndices),
                                 glyphPositionsCG, charsCount);
#if CGFLOAT_IS_DOUBLE
  delete[] glyphPositionsCG;
#endif
  CGContextRestoreGState(context);
  return true;
}
