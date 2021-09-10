// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/win32/cgdi_plus_ext.h"

#include <windows.h>

#include <objidl.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_fillrenderoptions.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "core/fxge/win32/cwin32_platform.h"
#include "third_party/base/span.h"
#include "third_party/base/stl_util.h"

// Has to come before gdiplus.h
namespace Gdiplus {
using std::max;
using std::min;
}  // namespace Gdiplus

#include <gdiplus.h>  // NOLINT

namespace {

enum {
  FuncId_GdipCreatePath2,
  FuncId_GdipSetPenDashArray,
  FuncId_GdipSetPenLineJoin,
  FuncId_GdipCreateFromHDC,
  FuncId_GdipSetPageUnit,
  FuncId_GdipSetSmoothingMode,
  FuncId_GdipCreateSolidFill,
  FuncId_GdipFillPath,
  FuncId_GdipDeleteBrush,
  FuncId_GdipCreatePen1,
  FuncId_GdipSetPenMiterLimit,
  FuncId_GdipDrawPath,
  FuncId_GdipDeletePen,
  FuncId_GdipDeletePath,
  FuncId_GdipDeleteGraphics,
  FuncId_GdipDisposeImage,
  FuncId_GdipCreateBitmapFromScan0,
  FuncId_GdipSetImagePalette,
  FuncId_GdipSetInterpolationMode,
  FuncId_GdipDrawImagePointsI,
  FuncId_GdiplusStartup,
  FuncId_GdipDrawLineI,
  FuncId_GdipCreatePath,
  FuncId_GdipSetPathFillMode,
  FuncId_GdipSetClipRegion,
  FuncId_GdipWidenPath,
  FuncId_GdipAddPathLine,
  FuncId_GdipAddPathRectangle,
  FuncId_GdipDeleteRegion,
  FuncId_GdipSetPenLineCap197819,
  FuncId_GdipSetPenDashOffset,
  FuncId_GdipCreateMatrix2,
  FuncId_GdipDeleteMatrix,
  FuncId_GdipSetWorldTransform,
  FuncId_GdipSetPixelOffsetMode,
};

LPCSTR g_GdipFuncNames[] = {
    "GdipCreatePath2",
    "GdipSetPenDashArray",
    "GdipSetPenLineJoin",
    "GdipCreateFromHDC",
    "GdipSetPageUnit",
    "GdipSetSmoothingMode",
    "GdipCreateSolidFill",
    "GdipFillPath",
    "GdipDeleteBrush",
    "GdipCreatePen1",
    "GdipSetPenMiterLimit",
    "GdipDrawPath",
    "GdipDeletePen",
    "GdipDeletePath",
    "GdipDeleteGraphics",
    "GdipDisposeImage",
    "GdipCreateBitmapFromScan0",
    "GdipSetImagePalette",
    "GdipSetInterpolationMode",
    "GdipDrawImagePointsI",
    "GdiplusStartup",
    "GdipDrawLineI",
    "GdipCreatePath",
    "GdipSetPathFillMode",
    "GdipSetClipRegion",
    "GdipWidenPath",
    "GdipAddPathLine",
    "GdipAddPathRectangle",
    "GdipDeleteRegion",
    "GdipSetPenLineCap197819",
    "GdipSetPenDashOffset",
    "GdipCreateMatrix2",
    "GdipDeleteMatrix",
    "GdipSetWorldTransform",
    "GdipSetPixelOffsetMode",
};
static_assert(pdfium::size(g_GdipFuncNames) ==
                  static_cast<size_t>(FuncId_GdipSetPixelOffsetMode) + 1,
              "g_GdipFuncNames has wrong size");

typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipCreatePath2)(
    GDIPCONST Gdiplus::GpPointF*,
    GDIPCONST BYTE*,
    INT,
    Gdiplus::GpFillMode,
    Gdiplus::GpPath** path);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetPenDashArray)(
    Gdiplus::GpPen* pen,
    GDIPCONST Gdiplus::REAL* dash,
    INT count);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetPenLineJoin)(
    Gdiplus::GpPen* pen,
    Gdiplus::GpLineJoin lineJoin);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipCreateFromHDC)(
    HDC hdc,
    Gdiplus::GpGraphics** graphics);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetPageUnit)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::GpUnit unit);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetSmoothingMode)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::SmoothingMode smoothingMode);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipCreateSolidFill)(
    Gdiplus::ARGB color,
    Gdiplus::GpSolidFill** brush);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipFillPath)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::GpBrush* brush,
    Gdiplus::GpPath* path);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDeleteBrush)(
    Gdiplus::GpBrush* brush);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipCreatePen1)(
    Gdiplus::ARGB color,
    Gdiplus::REAL width,
    Gdiplus::GpUnit unit,
    Gdiplus::GpPen** pen);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetPenMiterLimit)(
    Gdiplus::GpPen* pen,
    Gdiplus::REAL miterLimit);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDrawPath)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::GpPen* pen,
    Gdiplus::GpPath* path);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDeletePen)(
    Gdiplus::GpPen* pen);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDeletePath)(
    Gdiplus::GpPath* path);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDeleteGraphics)(
    Gdiplus::GpGraphics* graphics);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDisposeImage)(
    Gdiplus::GpImage* image);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipCreateBitmapFromScan0)(
    INT width,
    INT height,
    INT stride,
    Gdiplus::PixelFormat format,
    BYTE* scan0,
    Gdiplus::GpBitmap** bitmap);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetImagePalette)(
    Gdiplus::GpImage* image,
    GDIPCONST Gdiplus::ColorPalette* palette);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetInterpolationMode)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::InterpolationMode interpolationMode);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDrawImagePointsI)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::GpImage* image,
    GDIPCONST Gdiplus::GpPoint* dstpoints,
    INT count);
typedef Gdiplus::Status(WINAPI* FuncType_GdiplusStartup)(
    OUT uintptr_t* token,
    const Gdiplus::GdiplusStartupInput* input,
    OUT Gdiplus::GdiplusStartupOutput* output);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDrawLineI)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::GpPen* pen,
    int x1,
    int y1,
    int x2,
    int y2);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipCreatePath)(
    Gdiplus::GpFillMode brushMode,
    Gdiplus::GpPath** path);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetPathFillMode)(
    Gdiplus::GpPath* path,
    Gdiplus::GpFillMode fillmode);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetClipRegion)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::GpRegion* region,
    Gdiplus::CombineMode combineMode);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipWidenPath)(
    Gdiplus::GpPath* nativePath,
    Gdiplus::GpPen* pen,
    Gdiplus::GpMatrix* matrix,
    Gdiplus::REAL flatness);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipAddPathLine)(
    Gdiplus::GpPath* path,
    Gdiplus::REAL x1,
    Gdiplus::REAL y1,
    Gdiplus::REAL x2,
    Gdiplus::REAL y2);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipAddPathRectangle)(
    Gdiplus::GpPath* path,
    Gdiplus::REAL x,
    Gdiplus::REAL y,
    Gdiplus::REAL width,
    Gdiplus::REAL height);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDeleteRegion)(
    Gdiplus::GpRegion* region);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetPenLineCap197819)(
    Gdiplus::GpPen* pen,
    Gdiplus::GpLineCap startCap,
    Gdiplus::GpLineCap endCap,
    Gdiplus::GpDashCap dashCap);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetPenDashOffset)(
    Gdiplus::GpPen* pen,
    Gdiplus::REAL offset);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipCreateMatrix2)(
    Gdiplus::REAL m11,
    Gdiplus::REAL m12,
    Gdiplus::REAL m21,
    Gdiplus::REAL m22,
    Gdiplus::REAL dx,
    Gdiplus::REAL dy,
    Gdiplus::GpMatrix** matrix);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipDeleteMatrix)(
    Gdiplus::GpMatrix* matrix);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetWorldTransform)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::GpMatrix* matrix);
typedef Gdiplus::GpStatus(WINGDIPAPI* FuncType_GdipSetPixelOffsetMode)(
    Gdiplus::GpGraphics* graphics,
    Gdiplus::PixelOffsetMode pixelOffsetMode);
#define CallFunc(funcname)               \
  reinterpret_cast<FuncType_##funcname>( \
      GdiplusExt.m_Functions[FuncId_##funcname])

Gdiplus::GpFillMode FillType2Gdip(CFX_FillRenderOptions::FillType fill_type) {
  return fill_type == CFX_FillRenderOptions::FillType::kEvenOdd
             ? Gdiplus::FillModeAlternate
             : Gdiplus::FillModeWinding;
}

const CGdiplusExt& GetGdiplusExt() {
  auto* pData =
      static_cast<CWin32Platform*>(CFX_GEModule::Get()->GetPlatform());
  return pData->m_GdiplusExt;
}

Gdiplus::GpBrush* GdipCreateBrushImpl(DWORD argb) {
  const CGdiplusExt& GdiplusExt = GetGdiplusExt();
  Gdiplus::GpSolidFill* solidBrush = nullptr;
  CallFunc(GdipCreateSolidFill)((Gdiplus::ARGB)argb, &solidBrush);
  return solidBrush;
}

void OutputImage(Gdiplus::GpGraphics* pGraphics,
                 const RetainPtr<CFX_DIBitmap>& pBitmap,
                 const FX_RECT* pSrcRect,
                 int dest_left,
                 int dest_top,
                 int dest_width,
                 int dest_height) {
  int src_width = pSrcRect->Width(), src_height = pSrcRect->Height();
  const CGdiplusExt& GdiplusExt = GetGdiplusExt();
  if (pBitmap->GetBPP() == 1 && (pSrcRect->left % 8)) {
    FX_RECT new_rect(0, 0, src_width, src_height);
    RetainPtr<CFX_DIBitmap> pCloned = pBitmap->Clone(pSrcRect);
    if (!pCloned)
      return;
    OutputImage(pGraphics, pCloned, &new_rect, dest_left, dest_top, dest_width,
                dest_height);
    return;
  }
  int src_pitch = pBitmap->GetPitch();
  uint8_t* scan0 = pBitmap->GetBuffer() + pSrcRect->top * src_pitch +
                   pBitmap->GetBPP() * pSrcRect->left / 8;
  Gdiplus::GpBitmap* bitmap = nullptr;
  switch (pBitmap->GetFormat()) {
    case FXDIB_Format::kArgb:
      CallFunc(GdipCreateBitmapFromScan0)(src_width, src_height, src_pitch,
                                          PixelFormat32bppARGB, scan0, &bitmap);
      break;
    case FXDIB_Format::kRgb32:
      CallFunc(GdipCreateBitmapFromScan0)(src_width, src_height, src_pitch,
                                          PixelFormat32bppRGB, scan0, &bitmap);
      break;
    case FXDIB_Format::kRgb:
      CallFunc(GdipCreateBitmapFromScan0)(src_width, src_height, src_pitch,
                                          PixelFormat24bppRGB, scan0, &bitmap);
      break;
    case FXDIB_Format::k8bppRgb: {
      CallFunc(GdipCreateBitmapFromScan0)(src_width, src_height, src_pitch,
                                          PixelFormat8bppIndexed, scan0,
                                          &bitmap);
      UINT pal[258];
      pal[0] = 0;
      pal[1] = 256;
      for (int i = 0; i < 256; i++)
        pal[i + 2] = pBitmap->GetPaletteArgb(i);
      CallFunc(GdipSetImagePalette)(bitmap, (Gdiplus::ColorPalette*)pal);
      break;
    }
    case FXDIB_Format::k1bppRgb: {
      CallFunc(GdipCreateBitmapFromScan0)(src_width, src_height, src_pitch,
                                          PixelFormat1bppIndexed, scan0,
                                          &bitmap);
      break;
    }
  }
  if (dest_height < 0) {
    dest_height--;
  }
  if (dest_width < 0) {
    dest_width--;
  }
  Gdiplus::Point destinationPoints[] = {
      Gdiplus::Point(dest_left, dest_top),
      Gdiplus::Point(dest_left + dest_width, dest_top),
      Gdiplus::Point(dest_left, dest_top + dest_height)};
  CallFunc(GdipDrawImagePointsI)(pGraphics, bitmap, destinationPoints, 3);
  CallFunc(GdipDisposeImage)(bitmap);
}

Gdiplus::GpPen* GdipCreatePenImpl(const CFX_GraphStateData* pGraphState,
                                  const CFX_Matrix* pMatrix,
                                  DWORD argb,
                                  bool bTextMode) {
  const CGdiplusExt& GdiplusExt = GetGdiplusExt();
  float width = pGraphState->m_LineWidth;
  if (!bTextMode) {
    float unit = pMatrix
                     ? 1.0f / ((pMatrix->GetXUnit() + pMatrix->GetYUnit()) / 2)
                     : 1.0f;
    width = std::max(width, unit);
  }
  Gdiplus::GpPen* pPen = nullptr;
  CallFunc(GdipCreatePen1)((Gdiplus::ARGB)argb, width, Gdiplus::UnitWorld,
                           &pPen);
  Gdiplus::LineCap lineCap = Gdiplus::LineCapFlat;
  Gdiplus::DashCap dashCap = Gdiplus::DashCapFlat;
  bool bDashExtend = false;
  switch (pGraphState->m_LineCap) {
    case CFX_GraphStateData::LineCapButt:
      lineCap = Gdiplus::LineCapFlat;
      break;
    case CFX_GraphStateData::LineCapRound:
      lineCap = Gdiplus::LineCapRound;
      dashCap = Gdiplus::DashCapRound;
      bDashExtend = true;
      break;
    case CFX_GraphStateData::LineCapSquare:
      lineCap = Gdiplus::LineCapSquare;
      bDashExtend = true;
      break;
  }
  CallFunc(GdipSetPenLineCap197819)(pPen, lineCap, lineCap, dashCap);
  Gdiplus::LineJoin lineJoin = Gdiplus::LineJoinMiterClipped;
  switch (pGraphState->m_LineJoin) {
    case CFX_GraphStateData::LineJoinMiter:
      lineJoin = Gdiplus::LineJoinMiterClipped;
      break;
    case CFX_GraphStateData::LineJoinRound:
      lineJoin = Gdiplus::LineJoinRound;
      break;
    case CFX_GraphStateData::LineJoinBevel:
      lineJoin = Gdiplus::LineJoinBevel;
      break;
  }
  CallFunc(GdipSetPenLineJoin)(pPen, lineJoin);
  if (!pGraphState->m_DashArray.empty()) {
    float* pDashArray =
        FX_Alloc(float, FxAlignToBoundary<2>(pGraphState->m_DashArray.size()));
    int nCount = 0;
    float on_leftover = 0;
    float off_leftover = 0;
    for (size_t i = 0; i < pGraphState->m_DashArray.size(); i += 2) {
      float on_phase = pGraphState->m_DashArray[i];
      float off_phase;
      if (i == pGraphState->m_DashArray.size() - 1)
        off_phase = on_phase;
      else
        off_phase = pGraphState->m_DashArray[i + 1];
      on_phase /= width;
      off_phase /= width;
      if (on_phase + off_phase <= 0.00002f) {
        on_phase = 0.1f;
        off_phase = 0.1f;
      }
      if (bDashExtend) {
        if (off_phase < 1)
          off_phase = 0;
        else
          --off_phase;
        ++on_phase;
      }
      if (on_phase == 0 || off_phase == 0) {
        if (nCount == 0) {
          on_leftover += on_phase;
          off_leftover += off_phase;
        } else {
          pDashArray[nCount - 2] += on_phase;
          pDashArray[nCount - 1] += off_phase;
        }
      } else {
        pDashArray[nCount++] = on_phase + on_leftover;
        on_leftover = 0;
        pDashArray[nCount++] = off_phase + off_leftover;
        off_leftover = 0;
      }
    }
    CallFunc(GdipSetPenDashArray)(pPen, pDashArray, nCount);
    float phase = pGraphState->m_DashPhase;
    if (bDashExtend) {
      if (phase < 0.5f)
        phase = 0;
      else
        phase -= 0.5f;
    }
    CallFunc(GdipSetPenDashOffset)(pPen, phase);
    FX_Free(pDashArray);
    pDashArray = nullptr;
  }
  CallFunc(GdipSetPenMiterLimit)(pPen, pGraphState->m_MiterLimit);
  return pPen;
}

Optional<std::pair<size_t, size_t>> IsSmallTriangle(
    pdfium::span<const Gdiplus::PointF> points,
    const CFX_Matrix* pMatrix) {
  static constexpr size_t kPairs[3][2] = {{1, 2}, {0, 2}, {0, 1}};
  for (size_t i = 0; i < pdfium::size(kPairs); ++i) {
    size_t pair1 = kPairs[i][0];
    size_t pair2 = kPairs[i][1];

    CFX_PointF p1(points[pair1].X, points[pair1].Y);
    CFX_PointF p2(points[pair2].X, points[pair2].Y);
    if (pMatrix) {
      p1 = pMatrix->Transform(p1);
      p2 = pMatrix->Transform(p2);
    }

    CFX_PointF diff = p1 - p2;
    float distance_square = (diff.x * diff.x) + (diff.y * diff.y);
    if (distance_square < 2.25f)
      return std::make_pair(i, pair1);
  }
  return {};
}

class GpStream final : public IStream {
 public:
  GpStream() = default;
  ~GpStream() = default;

  // IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void** ppvObject) override {
    if (iid == __uuidof(IUnknown) || iid == __uuidof(IStream) ||
        iid == __uuidof(ISequentialStream)) {
      *ppvObject = static_cast<IStream*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override {
    return (ULONG)InterlockedIncrement(&m_RefCount);
  }
  ULONG STDMETHODCALLTYPE Release() override {
    ULONG res = (ULONG)InterlockedDecrement(&m_RefCount);
    if (res == 0) {
      delete this;
    }
    return res;
  }

  // ISequentialStream
  HRESULT STDMETHODCALLTYPE Read(void* output,
                                 ULONG cb,
                                 ULONG* pcbRead) override {
    if (pcbRead)
      *pcbRead = 0;

    if (m_ReadPos >= m_InterStream.tellp())
      return HRESULT_FROM_WIN32(ERROR_END_OF_MEDIA);

    size_t bytes_left = m_InterStream.tellp() - m_ReadPos;
    size_t bytes_out =
        std::min(pdfium::base::checked_cast<size_t>(cb), bytes_left);
    memcpy(output, m_InterStream.str().c_str() + m_ReadPos, bytes_out);
    m_ReadPos += bytes_out;
    if (pcbRead)
      *pcbRead = (ULONG)bytes_out;

    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Write(const void* input,
                                  ULONG cb,
                                  ULONG* pcbWritten) override {
    if (cb <= 0) {
      if (pcbWritten)
        *pcbWritten = 0;
      return S_OK;
    }
    m_InterStream.write(reinterpret_cast<const char*>(input), cb);
    if (pcbWritten)
      *pcbWritten = cb;
    return S_OK;
  }

  // IStream
  HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE CopyTo(IStream*,
                                   ULARGE_INTEGER,
                                   ULARGE_INTEGER*,
                                   ULARGE_INTEGER*) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE Revert() override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER,
                                       ULARGE_INTEGER,
                                       DWORD) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER,
                                         ULARGE_INTEGER,
                                         DWORD) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE Clone(IStream** stream) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER liDistanceToMove,
                                 DWORD dwOrigin,
                                 ULARGE_INTEGER* lpNewFilePointer) override {
    std::streamoff start;
    std::streamoff new_read_position;
    switch (dwOrigin) {
      case STREAM_SEEK_SET:
        start = 0;
        break;
      case STREAM_SEEK_CUR:
        start = m_ReadPos;
        break;
      case STREAM_SEEK_END:
        if (m_InterStream.tellp() < 0)
          return STG_E_SEEKERROR;
        start = m_InterStream.tellp();
        break;
      default:
        return STG_E_INVALIDFUNCTION;
    }
    new_read_position = start + liDistanceToMove.QuadPart;
    if (new_read_position > m_InterStream.tellp())
      return STG_E_SEEKERROR;

    m_ReadPos = new_read_position;
    if (lpNewFilePointer)
      lpNewFilePointer->QuadPart = m_ReadPos;

    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Stat(STATSTG* pStatstg,
                                 DWORD grfStatFlag) override {
    if (!pStatstg)
      return STG_E_INVALIDFUNCTION;

    ZeroMemory(pStatstg, sizeof(STATSTG));

    if (m_InterStream.tellp() < 0)
      return STG_E_SEEKERROR;

    pStatstg->cbSize.QuadPart = m_InterStream.tellp();
    return S_OK;
  }

 private:
  LONG m_RefCount = 1;
  std::streamoff m_ReadPos = 0;
  std::ostringstream m_InterStream;
};

}  // namespace

CGdiplusExt::CGdiplusExt() = default;

CGdiplusExt::~CGdiplusExt() {
  FreeLibrary(m_GdiModule);
  FreeLibrary(m_hModule);
}

void CGdiplusExt::Load() {
  char buf[MAX_PATH];
  GetSystemDirectoryA(buf, MAX_PATH);
  ByteString dllpath = buf;
  dllpath += "\\GDIPLUS.DLL";
  m_hModule = LoadLibraryA(dllpath.c_str());
  if (!m_hModule)
    return;

  m_Functions.resize(pdfium::size(g_GdipFuncNames));
  for (size_t i = 0; i < pdfium::size(g_GdipFuncNames); ++i) {
    m_Functions[i] = GetProcAddress(m_hModule, g_GdipFuncNames[i]);
    if (!m_Functions[i]) {
      m_hModule = nullptr;
      return;
    }
  }

  uintptr_t gdiplusToken;
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  ((FuncType_GdiplusStartup)m_Functions[FuncId_GdiplusStartup])(
      &gdiplusToken, &gdiplusStartupInput, nullptr);
  m_GdiModule = LoadLibraryA("GDI32.DLL");
}

bool CGdiplusExt::StretchDIBits(HDC hDC,
                                const RetainPtr<CFX_DIBitmap>& pBitmap,
                                int dest_left,
                                int dest_top,
                                int dest_width,
                                int dest_height,
                                const FX_RECT* pClipRect,
                                const FXDIB_ResampleOptions& options) {
  Gdiplus::GpGraphics* pGraphics;
  const CGdiplusExt& GdiplusExt = GetGdiplusExt();
  CallFunc(GdipCreateFromHDC)(hDC, &pGraphics);
  CallFunc(GdipSetPageUnit)(pGraphics, Gdiplus::UnitPixel);
  if (options.bNoSmoothing) {
    CallFunc(GdipSetInterpolationMode)(
        pGraphics, Gdiplus::InterpolationModeNearestNeighbor);
  } else if (pBitmap->GetWidth() > abs(dest_width) / 2 ||
             pBitmap->GetHeight() > abs(dest_height) / 2) {
    CallFunc(GdipSetInterpolationMode)(pGraphics,
                                       Gdiplus::InterpolationModeHighQuality);
  } else {
    CallFunc(GdipSetInterpolationMode)(pGraphics,
                                       Gdiplus::InterpolationModeBilinear);
  }
  FX_RECT src_rect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
  OutputImage(pGraphics, pBitmap, &src_rect, dest_left, dest_top, dest_width,
              dest_height);
  CallFunc(GdipDeleteGraphics)(pGraphics);
  CallFunc(GdipDeleteGraphics)(pGraphics);
  return true;
}

bool CGdiplusExt::DrawPath(HDC hDC,
                           const CFX_PathData* pPathData,
                           const CFX_Matrix* pObject2Device,
                           const CFX_GraphStateData* pGraphState,
                           uint32_t fill_argb,
                           uint32_t stroke_argb,
                           const CFX_FillRenderOptions& fill_options) {
  pdfium::span<const FX_PATHPOINT> points = pPathData->GetPoints();
  if (points.empty())
    return true;

  Gdiplus::GpGraphics* pGraphics = nullptr;
  const CGdiplusExt& GdiplusExt = GetGdiplusExt();
  CallFunc(GdipCreateFromHDC)(hDC, &pGraphics);
  CallFunc(GdipSetPageUnit)(pGraphics, Gdiplus::UnitPixel);
  CallFunc(GdipSetPixelOffsetMode)(pGraphics, Gdiplus::PixelOffsetModeHalf);
  Gdiplus::GpMatrix* pMatrix = nullptr;
  if (pObject2Device) {
    CallFunc(GdipCreateMatrix2)(pObject2Device->a, pObject2Device->b,
                                pObject2Device->c, pObject2Device->d,
                                pObject2Device->e, pObject2Device->f, &pMatrix);
    CallFunc(GdipSetWorldTransform)(pGraphics, pMatrix);
  }
  std::vector<Gdiplus::PointF> gp_points(points.size());
  std::vector<BYTE> gp_types(points.size());
  int nSubPathes = 0;
  bool bSubClose = false;
  int pos_subclose = 0;
  bool bSmooth = false;
  int startpoint = 0;
  for (size_t i = 0; i < points.size(); ++i) {
    gp_points[i].X = points[i].m_Point.x;
    gp_points[i].Y = points[i].m_Point.y;

    CFX_PointF pos = points[i].m_Point;
    if (pObject2Device)
      pos = pObject2Device->Transform(pos);

    if (pos.x > 50000.0f)
      gp_points[i].X = 50000.0f;
    if (pos.x < -50000.0f)
      gp_points[i].X = -50000.0f;
    if (pos.y > 50000.0f)
      gp_points[i].Y = 50000.0f;
    if (pos.y < -50000.0f)
      gp_points[i].Y = -50000.0f;

    FXPT_TYPE point_type = points[i].m_Type;
    if (point_type == FXPT_TYPE::MoveTo) {
      gp_types[i] = Gdiplus::PathPointTypeStart;
      nSubPathes++;
      bSubClose = false;
      startpoint = i;
    } else if (point_type == FXPT_TYPE::LineTo) {
      gp_types[i] = Gdiplus::PathPointTypeLine;
      if (points[i - 1].IsTypeAndOpen(FXPT_TYPE::MoveTo) &&
          (i == points.size() - 1 ||
           points[i + 1].IsTypeAndOpen(FXPT_TYPE::MoveTo)) &&
          gp_points[i].Y == gp_points[i - 1].Y &&
          gp_points[i].X == gp_points[i - 1].X) {
        gp_points[i].X += 0.01f;
        continue;
      }
      if (!bSmooth && gp_points[i].X != gp_points[i - 1].X &&
          gp_points[i].Y != gp_points[i - 1].Y) {
        bSmooth = true;
      }
    } else if (point_type == FXPT_TYPE::BezierTo) {
      gp_types[i] = Gdiplus::PathPointTypeBezier;
      bSmooth = true;
    }
    if (points[i].m_CloseFigure) {
      if (bSubClose)
        gp_types[pos_subclose] &= ~Gdiplus::PathPointTypeCloseSubpath;
      else
        bSubClose = true;
      pos_subclose = i;
      gp_types[i] |= Gdiplus::PathPointTypeCloseSubpath;
      if (!bSmooth && gp_points[i].X != gp_points[startpoint].X &&
          gp_points[i].Y != gp_points[startpoint].Y) {
        bSmooth = true;
      }
    }
  }
  const bool fill =
      fill_options.fill_type != CFX_FillRenderOptions::FillType::kNoFill;
  if (fill_options.aliased_path) {
    bSmooth = false;
    CallFunc(GdipSetSmoothingMode)(pGraphics, Gdiplus::SmoothingModeNone);
  } else if (!fill_options.full_cover) {
    if (!bSmooth && fill)
      bSmooth = true;

    if (bSmooth || (pGraphState && pGraphState->m_LineWidth > 2)) {
      CallFunc(GdipSetSmoothingMode)(pGraphics,
                                     Gdiplus::SmoothingModeAntiAlias);
    }
  }
  if (points.size() == 4 && !pGraphState) {
    auto indices = IsSmallTriangle(gp_points, pObject2Device);
    if (indices.has_value()) {
      size_t v1;
      size_t v2;
      std::tie(v1, v2) = indices.value();
      Gdiplus::GpPen* pPen = nullptr;
      CallFunc(GdipCreatePen1)(fill_argb, 1.0f, Gdiplus::UnitPixel, &pPen);
      CallFunc(GdipDrawLineI)(pGraphics, pPen, FXSYS_roundf(gp_points[v1].X),
                              FXSYS_roundf(gp_points[v1].Y),
                              FXSYS_roundf(gp_points[v2].X),
                              FXSYS_roundf(gp_points[v2].Y));
      CallFunc(GdipDeletePen)(pPen);
      return true;
    }
  }
  Gdiplus::GpPath* pGpPath = nullptr;
  const Gdiplus::GpFillMode gp_fill_mode =
      FillType2Gdip(fill_options.fill_type);
  CallFunc(GdipCreatePath2)(gp_points.data(), gp_types.data(), points.size(),
                            gp_fill_mode, &pGpPath);
  if (!pGpPath) {
    if (pMatrix)
      CallFunc(GdipDeleteMatrix)(pMatrix);

    CallFunc(GdipDeleteGraphics)(pGraphics);
    return false;
  }
  if (fill) {
    Gdiplus::GpBrush* pBrush = GdipCreateBrushImpl(fill_argb);
    CallFunc(GdipSetPathFillMode)(pGpPath, gp_fill_mode);
    CallFunc(GdipFillPath)(pGraphics, pBrush, pGpPath);
    CallFunc(GdipDeleteBrush)(pBrush);
  }
  if (pGraphState && stroke_argb) {
    Gdiplus::GpPen* pPen =
        GdipCreatePenImpl(pGraphState, pObject2Device, stroke_argb,
                          fill_options.stroke_text_mode);
    if (nSubPathes == 1) {
      CallFunc(GdipDrawPath)(pGraphics, pPen, pGpPath);
    } else {
      int iStart = 0;
      for (size_t i = 0; i < points.size(); ++i) {
        if (i == points.size() - 1 ||
            gp_types[i + 1] == Gdiplus::PathPointTypeStart) {
          Gdiplus::GpPath* pSubPath;
          CallFunc(GdipCreatePath2)(&gp_points[iStart], &gp_types[iStart],
                                    i - iStart + 1, gp_fill_mode, &pSubPath);
          iStart = i + 1;
          CallFunc(GdipDrawPath)(pGraphics, pPen, pSubPath);
          CallFunc(GdipDeletePath)(pSubPath);
        }
      }
    }
    CallFunc(GdipDeletePen)(pPen);
  }
  if (pMatrix)
    CallFunc(GdipDeleteMatrix)(pMatrix);
  CallFunc(GdipDeletePath)(pGpPath);
  CallFunc(GdipDeleteGraphics)(pGraphics);
  return true;
}
