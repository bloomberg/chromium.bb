// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_WIN32_CFX_PSRENDERER_H_
#define CORE_FXGE_WIN32_CFX_PSRENDERER_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_stream.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/unowned_ptr.h"
#include "core/fxge/cfx_graphstatedata.h"

class CCodec_ModuleMgr;
class CFX_DIBBase;
class CFX_FaceCache;
class CFX_Font;
class CFX_FontCache;
class CFX_Matrix;
class CFX_PathData;
class CPSFont;
class TextCharPos;
struct FXDIB_ResampleOptions;

class CFX_PSRenderer {
 public:
  explicit CFX_PSRenderer(CCodec_ModuleMgr* pModuleMgr);
  ~CFX_PSRenderer();

  void Init(const RetainPtr<IFX_RetainableWriteStream>& stream,
            int pslevel,
            int width,
            int height,
            bool bCmykOutput);
  bool StartRendering();
  void EndRendering();
  void SaveState();
  void RestoreState(bool bKeepSaved);
  void SetClip_PathFill(const CFX_PathData* pPathData,
                        const CFX_Matrix* pObject2Device,
                        int fill_mode);
  void SetClip_PathStroke(const CFX_PathData* pPathData,
                          const CFX_Matrix* pObject2Device,
                          const CFX_GraphStateData* pGraphState);
  FX_RECT GetClipBox() { return m_ClipBox; }
  bool DrawPath(const CFX_PathData* pPathData,
                const CFX_Matrix* pObject2Device,
                const CFX_GraphStateData* pGraphState,
                uint32_t fill_color,
                uint32_t stroke_color,
                int fill_mode);
  bool SetDIBits(const RetainPtr<CFX_DIBBase>& pBitmap,
                 uint32_t color,
                 int dest_left,
                 int dest_top);
  bool StretchDIBits(const RetainPtr<CFX_DIBBase>& pBitmap,
                     uint32_t color,
                     int dest_left,
                     int dest_top,
                     int dest_width,
                     int dest_height,
                     const FXDIB_ResampleOptions& options);
  bool DrawDIBits(const RetainPtr<CFX_DIBBase>& pBitmap,
                  uint32_t color,
                  const CFX_Matrix& matrix,
                  const FXDIB_ResampleOptions& options);
  bool DrawText(int nChars,
                const TextCharPos* pCharPos,
                CFX_Font* pFont,
                const CFX_Matrix* pObject2Device,
                float font_size,
                uint32_t color);

 private:
  void OutputPath(const CFX_PathData* pPathData,
                  const CFX_Matrix* pObject2Device);
  void SetGraphState(const CFX_GraphStateData* pGraphState);
  void SetColor(uint32_t color);
  void FindPSFontGlyph(CFX_FaceCache* pFaceCache,
                       CFX_Font* pFont,
                       const TextCharPos& charpos,
                       int* ps_fontnum,
                       int* ps_glyphindex);
  void WritePSBinary(const uint8_t* data, int len);
  void WriteToStream(std::ostringstream* stringStream);

  bool m_bInited = false;
  bool m_bGraphStateSet = false;
  bool m_bCmykOutput;
  bool m_bColorSet = false;
  int m_PSLevel = 0;
  uint32_t m_LastColor = 0;
  FX_RECT m_ClipBox;
  CFX_GraphStateData m_CurGraphState;
  UnownedPtr<CCodec_ModuleMgr> m_pModuleMgr;
  RetainPtr<IFX_RetainableWriteStream> m_pStream;
  std::vector<std::unique_ptr<CPSFont>> m_PSFontList;
  std::vector<FX_RECT> m_ClipBoxStack;
};

#endif  // CORE_FXGE_WIN32_CFX_PSRENDERER_H_
