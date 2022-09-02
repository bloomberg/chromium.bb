// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/tiff/tiff_decoder.h"

#include <limits>
#include <memory>

#include "core/fxcodec/cfx_codec_memory.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxcodec/fx_codec_def.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxcrt/fx_stream.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "core/fxge/dib/fx_dib.h"
#include "third_party/base/check.h"
#include "third_party/base/notreached.h"
#include "third_party/base/numerics/safe_conversions.h"

extern "C" {
#include "third_party/libtiff/tiffiop.h"
}  // extern C

namespace {

// For use with std::unique_ptr<TIFF>.
struct TiffDeleter {
  inline void operator()(TIFF* context) { TIFFClose(context); }
};

}  // namespace

class CTiffContext final : public ProgressiveDecoderIface::Context {
 public:
  CTiffContext() = default;
  ~CTiffContext() override = default;

  bool InitDecoder(const RetainPtr<IFX_SeekableReadStream>& file_ptr);
  bool LoadFrameInfo(int32_t frame,
                     int32_t* width,
                     int32_t* height,
                     int32_t* comps,
                     int32_t* bpc,
                     CFX_DIBAttribute* pAttribute);
  bool Decode(const RetainPtr<CFX_DIBitmap>& pDIBitmap);

  RetainPtr<IFX_SeekableReadStream> io_in() const { return m_io_in; }
  uint32_t offset() const { return m_offset; }
  void set_offset(uint32_t offset) { m_offset = offset; }

 private:
  bool IsSupport(const RetainPtr<CFX_DIBitmap>& pDIBitmap) const;
  void SetPalette(const RetainPtr<CFX_DIBitmap>& pDIBitmap, uint16_t bps);
  bool Decode1bppRGB(const RetainPtr<CFX_DIBitmap>& pDIBitmap,
                     int32_t height,
                     int32_t width,
                     uint16_t bps,
                     uint16_t spp);
  bool Decode8bppRGB(const RetainPtr<CFX_DIBitmap>& pDIBitmap,
                     int32_t height,
                     int32_t width,
                     uint16_t bps,
                     uint16_t spp);
  bool Decode24bppRGB(const RetainPtr<CFX_DIBitmap>& pDIBitmap,
                      int32_t height,
                      int32_t width,
                      uint16_t bps,
                      uint16_t spp);

  RetainPtr<IFX_SeekableReadStream> m_io_in;
  uint32_t m_offset = 0;
  std::unique_ptr<TIFF, TiffDeleter> m_tif_ctx;
};

void* _TIFFcalloc(tmsize_t nmemb, tmsize_t siz) {
  return FXMEM_DefaultCalloc(nmemb, siz);
}

void* _TIFFmalloc(tmsize_t size) {
  return FXMEM_DefaultAlloc(size);
}

void _TIFFfree(void* ptr) {
  if (ptr)
    FXMEM_DefaultFree(ptr);
}

void* _TIFFrealloc(void* ptr, tmsize_t size) {
  return FXMEM_DefaultRealloc(ptr, size);
}

void _TIFFmemset(void* ptr, int val, tmsize_t size) {
  memset(ptr, val, static_cast<size_t>(size));
}

void _TIFFmemcpy(void* des, const void* src, tmsize_t size) {
  memcpy(des, src, static_cast<size_t>(size));
}

int _TIFFmemcmp(const void* ptr1, const void* ptr2, tmsize_t size) {
  return memcmp(ptr1, ptr2, static_cast<size_t>(size));
}

TIFFErrorHandler _TIFFwarningHandler = nullptr;
TIFFErrorHandler _TIFFerrorHandler = nullptr;

namespace {

tsize_t tiff_read(thandle_t context, tdata_t buf, tsize_t length) {
  CTiffContext* pTiffContext = reinterpret_cast<CTiffContext*>(context);
  FX_SAFE_UINT32 increment = pTiffContext->offset();
  increment += length;
  if (!increment.IsValid())
    return 0;

  FX_FILESIZE offset = pTiffContext->offset();
  if (!pTiffContext->io_in()->ReadBlockAtOffset(buf, offset, length))
    return 0;

  pTiffContext->set_offset(increment.ValueOrDie());
  if (offset + length > pTiffContext->io_in()->GetSize()) {
    return pdfium::base::checked_cast<tsize_t>(
        pTiffContext->io_in()->GetSize() - offset);
  }
  return length;
}

tsize_t tiff_write(thandle_t context, tdata_t buf, tsize_t length) {
  NOTREACHED();
  return 0;
}

toff_t tiff_seek(thandle_t context, toff_t offset, int whence) {
  CTiffContext* pTiffContext = reinterpret_cast<CTiffContext*>(context);
  FX_SAFE_FILESIZE safe_offset = offset;
  if (!safe_offset.IsValid())
    return static_cast<toff_t>(-1);
  FX_FILESIZE file_offset = safe_offset.ValueOrDie();

  switch (whence) {
    case 0: {
      if (file_offset > pTiffContext->io_in()->GetSize())
        return static_cast<toff_t>(-1);
      pTiffContext->set_offset(
          pdfium::base::checked_cast<uint32_t>(file_offset));
      return pTiffContext->offset();
    }
    case 1: {
      FX_SAFE_UINT32 new_increment = pTiffContext->offset();
      new_increment += file_offset;
      if (!new_increment.IsValid())
        return static_cast<toff_t>(-1);
      pTiffContext->set_offset(new_increment.ValueOrDie());
      return pTiffContext->offset();
    }
    case 2: {
      if (pTiffContext->io_in()->GetSize() < file_offset)
        return static_cast<toff_t>(-1);
      pTiffContext->set_offset(pdfium::base::checked_cast<uint32_t>(
          pTiffContext->io_in()->GetSize() - file_offset));
      return pTiffContext->offset();
    }
    default:
      return static_cast<toff_t>(-1);
  }
}

int tiff_close(thandle_t context) {
  return 0;
}

toff_t tiff_get_size(thandle_t context) {
  CTiffContext* pTiffContext = reinterpret_cast<CTiffContext*>(context);
  return static_cast<toff_t>(pTiffContext->io_in()->GetSize());
}

int tiff_map(thandle_t context, tdata_t*, toff_t*) {
  return 0;
}

void tiff_unmap(thandle_t context, tdata_t, toff_t) {}

TIFF* tiff_open(void* context, const char* mode) {
  TIFF* tif = TIFFClientOpen("Tiff Image", mode, (thandle_t)context, tiff_read,
                             tiff_write, tiff_seek, tiff_close, tiff_get_size,
                             tiff_map, tiff_unmap);
  if (tif) {
    tif->tif_fd = (int)(intptr_t)context;
  }
  return tif;
}

void TiffBGRA2RGBA(uint8_t* pBuf, int32_t pixel, int32_t spp) {
  for (int32_t n = 0; n < pixel; n++) {
    uint8_t tmp = pBuf[0];
    pBuf[0] = pBuf[2];
    pBuf[2] = tmp;
    pBuf += spp;
  }
}

}  // namespace

bool CTiffContext::InitDecoder(
    const RetainPtr<IFX_SeekableReadStream>& file_ptr) {
  m_io_in = file_ptr;
  m_tif_ctx.reset(tiff_open(this, "r"));
  return !!m_tif_ctx;
}

bool CTiffContext::LoadFrameInfo(int32_t frame,
                                 int32_t* width,
                                 int32_t* height,
                                 int32_t* comps,
                                 int32_t* bpc,
                                 CFX_DIBAttribute* pAttribute) {
  if (!TIFFSetDirectory(m_tif_ctx.get(), (uint16)frame))
    return false;

  uint32_t tif_width = 0;
  uint32_t tif_height = 0;
  uint16_t tif_comps = 0;
  uint16_t tif_bpc = 0;
  uint32_t tif_rps = 0;
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_IMAGEWIDTH, &tif_width);
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_IMAGELENGTH, &tif_height);
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_SAMPLESPERPIXEL, &tif_comps);
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_BITSPERSAMPLE, &tif_bpc);
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_ROWSPERSTRIP, &tif_rps);

  uint16_t tif_resunit = 0;
  if (TIFFGetField(m_tif_ctx.get(), TIFFTAG_RESOLUTIONUNIT, &tif_resunit)) {
    pAttribute->m_wDPIUnit =
        static_cast<CFX_DIBAttribute::ResUnit>(tif_resunit - 1);
  } else {
    pAttribute->m_wDPIUnit = CFX_DIBAttribute::kResUnitInch;
  }

  float tif_xdpi = 0.0f;
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_XRESOLUTION, &tif_xdpi);
  if (tif_xdpi)
    pAttribute->m_nXDPI = static_cast<int32_t>(tif_xdpi + 0.5f);

  float tif_ydpi = 0.0f;
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_YRESOLUTION, &tif_ydpi);
  if (tif_ydpi)
    pAttribute->m_nYDPI = static_cast<int32_t>(tif_ydpi + 0.5f);

  FX_SAFE_INT32 checked_width = tif_width;
  FX_SAFE_INT32 checked_height = tif_height;
  if (!checked_width.IsValid() || !checked_height.IsValid())
    return false;

  *width = checked_width.ValueOrDie();
  *height = checked_height.ValueOrDie();
  *comps = tif_comps;
  *bpc = tif_bpc;
  if (tif_rps > tif_height) {
    tif_rps = tif_height;
    TIFFSetField(m_tif_ctx.get(), TIFFTAG_ROWSPERSTRIP, tif_rps);
  }
  return true;
}

bool CTiffContext::IsSupport(const RetainPtr<CFX_DIBitmap>& pDIBitmap) const {
  if (TIFFIsTiled(m_tif_ctx.get()))
    return false;

  uint16_t photometric = 0;
  if (!TIFFGetField(m_tif_ctx.get(), TIFFTAG_PHOTOMETRIC, &photometric))
    return false;

  switch (pDIBitmap->GetBPP()) {
    case 1:
    case 8:
      if (photometric != PHOTOMETRIC_PALETTE) {
        return false;
      }
      break;
    case 24:
      if (photometric != PHOTOMETRIC_RGB) {
        return false;
      }
      break;
    default:
      return false;
  }
  uint16_t planarconfig = 0;
  if (!TIFFGetFieldDefaulted(m_tif_ctx.get(), TIFFTAG_PLANARCONFIG,
                             &planarconfig))
    return false;

  return planarconfig != PLANARCONFIG_SEPARATE;
}

void CTiffContext::SetPalette(const RetainPtr<CFX_DIBitmap>& pDIBitmap,
                              uint16_t bps) {
  uint16_t* red_orig = nullptr;
  uint16_t* green_orig = nullptr;
  uint16_t* blue_orig = nullptr;
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_COLORMAP, &red_orig, &green_orig,
               &blue_orig);
  for (int32_t i = pdfium::base::checked_cast<int32_t>((1L << bps) - 1); i >= 0;
       i--) {
#define CVT(x) ((uint16_t)((x) >> 8))
    red_orig[i] = CVT(red_orig[i]);
    green_orig[i] = CVT(green_orig[i]);
    blue_orig[i] = CVT(blue_orig[i]);
#undef CVT
  }
  int32_t len = 1 << bps;
  for (int32_t index = 0; index < len; index++) {
    uint32_t r = red_orig[index] & 0xFF;
    uint32_t g = green_orig[index] & 0xFF;
    uint32_t b = blue_orig[index] & 0xFF;
    uint32_t color = (uint32_t)b | ((uint32_t)g << 8) | ((uint32_t)r << 16) |
                     (((uint32)0xffL) << 24);
    pDIBitmap->SetPaletteArgb(index, color);
  }
}

bool CTiffContext::Decode1bppRGB(const RetainPtr<CFX_DIBitmap>& pDIBitmap,
                                 int32_t height,
                                 int32_t width,
                                 uint16_t bps,
                                 uint16_t spp) {
  if (pDIBitmap->GetBPP() != 1 || spp != 1 || bps != 1 ||
      !IsSupport(pDIBitmap)) {
    return false;
  }
  SetPalette(pDIBitmap, bps);
  int32_t size = static_cast<int32_t>(TIFFScanlineSize(m_tif_ctx.get()));
  uint8_t* buf = (uint8_t*)_TIFFmalloc(size);
  if (!buf) {
    TIFFError(TIFFFileName(m_tif_ctx.get()), "No space for scanline buffer");
    return false;
  }
  for (int32_t row = 0; row < height; row++) {
    uint8_t* bitMapbuffer = pDIBitmap->GetWritableScanline(row).data();
    TIFFReadScanline(m_tif_ctx.get(), buf, row, 0);
    for (int32_t j = 0; j < size; j++) {
      bitMapbuffer[j] = buf[j];
    }
  }
  _TIFFfree(buf);
  return true;
}

bool CTiffContext::Decode8bppRGB(const RetainPtr<CFX_DIBitmap>& pDIBitmap,
                                 int32_t height,
                                 int32_t width,
                                 uint16_t bps,
                                 uint16_t spp) {
  if (pDIBitmap->GetBPP() != 8 || spp != 1 || (bps != 4 && bps != 8) ||
      !IsSupport(pDIBitmap)) {
    return false;
  }
  SetPalette(pDIBitmap, bps);
  int32_t size = static_cast<int32_t>(TIFFScanlineSize(m_tif_ctx.get()));
  uint8_t* buf = (uint8_t*)_TIFFmalloc(size);
  if (!buf) {
    TIFFError(TIFFFileName(m_tif_ctx.get()), "No space for scanline buffer");
    return false;
  }
  for (int32_t row = 0; row < height; row++) {
    uint8_t* bitMapbuffer = pDIBitmap->GetWritableScanline(row).data();
    TIFFReadScanline(m_tif_ctx.get(), buf, row, 0);
    for (int32_t j = 0; j < size; j++) {
      switch (bps) {
        case 4:
          bitMapbuffer[2 * j + 0] = (buf[j] & 0xF0) >> 4;
          bitMapbuffer[2 * j + 1] = (buf[j] & 0x0F) >> 0;
          break;
        case 8:
          bitMapbuffer[j] = buf[j];
          break;
      }
    }
  }
  _TIFFfree(buf);
  return true;
}

bool CTiffContext::Decode24bppRGB(const RetainPtr<CFX_DIBitmap>& pDIBitmap,
                                  int32_t height,
                                  int32_t width,
                                  uint16_t bps,
                                  uint16_t spp) {
  if (pDIBitmap->GetBPP() != 24 || !IsSupport(pDIBitmap))
    return false;

  int32_t size = static_cast<int32_t>(TIFFScanlineSize(m_tif_ctx.get()));
  uint8_t* buf = (uint8_t*)_TIFFmalloc(size);
  if (!buf) {
    TIFFError(TIFFFileName(m_tif_ctx.get()), "No space for scanline buffer");
    return false;
  }
  for (int32_t row = 0; row < height; row++) {
    uint8_t* bitMapbuffer = pDIBitmap->GetWritableScanline(row).data();
    TIFFReadScanline(m_tif_ctx.get(), buf, row, 0);
    for (int32_t j = 0; j < size - 2; j += 3) {
      bitMapbuffer[j + 0] = buf[j + 2];
      bitMapbuffer[j + 1] = buf[j + 1];
      bitMapbuffer[j + 2] = buf[j + 0];
    }
  }
  _TIFFfree(buf);
  return true;
}

bool CTiffContext::Decode(const RetainPtr<CFX_DIBitmap>& pDIBitmap) {
  uint32_t img_width = pDIBitmap->GetWidth();
  uint32_t img_height = pDIBitmap->GetHeight();
  uint32_t width = 0;
  uint32_t height = 0;
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_IMAGELENGTH, &height);
  if (img_width != width || img_height != height)
    return false;

  if (pDIBitmap->GetBPP() == 32) {
    uint16_t rotation = ORIENTATION_TOPLEFT;
    TIFFGetField(m_tif_ctx.get(), TIFFTAG_ORIENTATION, &rotation);
    if (TIFFReadRGBAImageOriented(m_tif_ctx.get(), img_width, img_height,
                                  (uint32*)pDIBitmap->GetBuffer(), rotation,
                                  1)) {
      for (uint32_t row = 0; row < img_height; row++) {
        uint8_t* row_buf = pDIBitmap->GetWritableScanline(row).data();
        TiffBGRA2RGBA(row_buf, img_width, 4);
      }
      return true;
    }
  }
  uint16_t spp = 0;
  uint16_t bps = 0;
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_SAMPLESPERPIXEL, &spp);
  TIFFGetField(m_tif_ctx.get(), TIFFTAG_BITSPERSAMPLE, &bps);
  FX_SAFE_UINT32 safe_bpp = bps;
  safe_bpp *= spp;
  if (!safe_bpp.IsValid())
    return false;
  uint32_t bpp = safe_bpp.ValueOrDie();
  if (bpp == 1)
    return Decode1bppRGB(pDIBitmap, height, width, bps, spp);
  if (bpp <= 8)
    return Decode8bppRGB(pDIBitmap, height, width, bps, spp);
  if (bpp <= 24)
    return Decode24bppRGB(pDIBitmap, height, width, bps, spp);
  return false;
}

namespace fxcodec {

// static
std::unique_ptr<ProgressiveDecoderIface::Context> TiffDecoder::CreateDecoder(
    const RetainPtr<IFX_SeekableReadStream>& file_ptr) {
  auto pDecoder = std::make_unique<CTiffContext>();
  if (!pDecoder->InitDecoder(file_ptr))
    return nullptr;

  return pDecoder;
}

// static
bool TiffDecoder::LoadFrameInfo(ProgressiveDecoderIface::Context* pContext,
                                int32_t frame,
                                int32_t* width,
                                int32_t* height,
                                int32_t* comps,
                                int32_t* bpc,
                                CFX_DIBAttribute* pAttribute) {
  DCHECK(pAttribute);

  auto* ctx = static_cast<CTiffContext*>(pContext);
  return ctx->LoadFrameInfo(frame, width, height, comps, bpc, pAttribute);
}

// static
bool TiffDecoder::Decode(ProgressiveDecoderIface::Context* pContext,
                         const RetainPtr<CFX_DIBitmap>& pDIBitmap) {
  auto* ctx = static_cast<CTiffContext*>(pContext);
  return ctx->Decode(pDIBitmap);
}

}  // namespace fxcodec
