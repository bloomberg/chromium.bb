// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/cpdf_modulemgr.h"

#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fxcodec/fx_codec.h"
#include "third_party/base/ptr_util.h"

#ifdef PDF_ENABLE_XFA_BMP
#include "core/fxcodec/codec/ccodec_bmpmodule.h"
#endif

#ifdef PDF_ENABLE_XFA_GIF
#include "core/fxcodec/codec/ccodec_gifmodule.h"
#endif

#ifdef PDF_ENABLE_XFA_PNG
#include "core/fxcodec/codec/ccodec_pngmodule.h"
#endif

#ifdef PDF_ENABLE_XFA_TIFF
#include "core/fxcodec/codec/ccodec_tiffmodule.h"
#endif

namespace {

CPDF_ModuleMgr* g_pDefaultMgr = nullptr;

}  // namespace

// static
CPDF_ModuleMgr* CPDF_ModuleMgr::Get() {
  if (!g_pDefaultMgr)
    g_pDefaultMgr = new CPDF_ModuleMgr;
  return g_pDefaultMgr;
}

// static
void CPDF_ModuleMgr::Destroy() {
  delete g_pDefaultMgr;
  g_pDefaultMgr = nullptr;
}

CPDF_ModuleMgr::CPDF_ModuleMgr() {}

CPDF_ModuleMgr::~CPDF_ModuleMgr() {}

void CPDF_ModuleMgr::Init() {
  InitCodecModule();
  InitPageModule();
  LoadEmbeddedMaps();
  LoadCodecModules();
}

void CPDF_ModuleMgr::LoadEmbeddedMaps() {
  LoadEmbeddedGB1CMaps();
  LoadEmbeddedJapan1CMaps();
  LoadEmbeddedCNS1CMaps();
  LoadEmbeddedKorea1CMaps();
}

void CPDF_ModuleMgr::LoadCodecModules() {
#ifdef PDF_ENABLE_XFA_BMP
  m_pCodecModule->SetBmpModule(pdfium::MakeUnique<CCodec_BmpModule>());
#endif

#ifdef PDF_ENABLE_XFA_GIF
  m_pCodecModule->SetGifModule(pdfium::MakeUnique<CCodec_GifModule>());
#endif

#ifdef PDF_ENABLE_XFA_PNG
  m_pCodecModule->SetPngModule(pdfium::MakeUnique<CCodec_PngModule>());
#endif

#ifdef PDF_ENABLE_XFA_TIFF
  m_pCodecModule->SetTiffModule(pdfium::MakeUnique<CCodec_TiffModule>());
#endif
}

void CPDF_ModuleMgr::InitCodecModule() {
  m_pCodecModule = pdfium::MakeUnique<CCodec_ModuleMgr>();
}

void CPDF_ModuleMgr::InitPageModule() {
  m_pPageModule = pdfium::MakeUnique<CPDF_PageModule>();
}

CCodec_FaxModule* CPDF_ModuleMgr::GetFaxModule() {
  return m_pCodecModule->GetFaxModule();
}

CCodec_JpegModule* CPDF_ModuleMgr::GetJpegModule() {
  return m_pCodecModule->GetJpegModule();
}

CCodec_JpxModule* CPDF_ModuleMgr::GetJpxModule() {
  return m_pCodecModule->GetJpxModule();
}

CCodec_Jbig2Module* CPDF_ModuleMgr::GetJbig2Module() {
  return m_pCodecModule->GetJbig2Module();
}

CCodec_IccModule* CPDF_ModuleMgr::GetIccModule() {
  return m_pCodecModule->GetIccModule();
}

CCodec_FlateModule* CPDF_ModuleMgr::GetFlateModule() {
  return m_pCodecModule->GetFlateModule();
}
