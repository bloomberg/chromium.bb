// Copyright 2020 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_THEME_CFWL_FONTMANAGER_H_
#define XFA_FWL_THEME_CFWL_FONTMANAGER_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/retain_ptr.h"

class CFGAS_GEFont;

class CFWL_FontManager final {
 public:
  static CFWL_FontManager* GetInstance();
  static void DestroyInstance();

  RetainPtr<CFGAS_GEFont> FindFont(WideStringView wsFontFamily,
                                   uint32_t dwFontStyles,
                                   uint16_t dwCodePage);

 private:
  class FontData final {
   public:
    FontData();
    ~FontData();

    bool Equal(WideStringView wsFontFamily,
               uint32_t dwFontStyles,
               uint16_t wCodePage);
    bool LoadFont(WideStringView wsFontFamily,
                  uint32_t dwFontStyles,
                  uint16_t wCodePage);
    RetainPtr<CFGAS_GEFont> GetFont() const;

   private:
    WideString m_wsFamily;
    uint32_t m_dwStyles = 0;
    uint32_t m_dwCodePage = 0;
    RetainPtr<CFGAS_GEFont> m_pFont;
  };

  CFWL_FontManager();
  ~CFWL_FontManager();

  std::vector<std::unique_ptr<FontData>> m_FontsArray;
};

#endif  // XFA_FWL_THEME_CFWL_FONTMANAGER_H_
