// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_THEMEPART_H_
#define XFA_FWL_CFWL_THEMEPART_H_

#include <stdint.h>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/mask.h"
#include "core/fxcrt/unowned_ptr.h"
#include "xfa/fwl/theme/cfwl_utils.h"

class CFWL_Widget;

enum class CFWL_PartState : uint16_t {
  kNormal = 0,
  kChecked = 1 << 1,
  kDefault = 1 << 2,
  kDisabled = 1 << 3,
  kFlagged = 1 << 4,
  kFocused = 1 << 5,
  kHightLight = 1 << 6,
  kHovered = 1 << 7,
  kNeutral = 1 << 9,
  kPressed = 1 << 10,
  kReadOnly = 1 << 11,
  kLSelected = 1 << 12,
  kRSelected = 1 << 13,
  kSelected = 1 << 14
};

class CFWL_ThemePart {
 public:
  enum class Part : uint8_t {
    kNone = 0,

    kBackArrow,
    kBackground,
    kBorder,
    kCaption,
    kCheck,
    kCheckBox,
    kCloseBox,
    kCombTextLine,
    kDateInBK,
    kDateInCircle,
    kDatesIn,
    kDatesOut,
    kDownButton,
    kDropDownButton,
    kForeArrow,
    kHSeparator,
    kHeadText,
    kHeader,
    kIcon,
    kImage,
    kLBtn,
    kListItem,
    kLowerTrack,
    kMinimizeBox,
    kMaximizeBox,
    kNarrowCaption,
    kRBtn,
    kThumb,
    kThumbBackArrow,
    kThumbForeArrow,
    kThumbLowerTrack,
    kThumbThumb,
    kThumbUpperTrack,
    kToday,
    kTodayCircle,
    kUpButton,
    kUpperTrack,
    kVSeparator,
    kWeek,
    kWeekNum,
    kWeekNumSep
  };

  explicit CFWL_ThemePart(CFWL_Widget* pWidget);
  ~CFWL_ThemePart();

  CFWL_Widget* GetWidget() const { return m_pWidget.Get(); }
  FWLTHEME_STATE GetThemeState() const;

  CFX_Matrix m_matrix;
  CFX_RectF m_PartRect;
  UnownedPtr<const CFX_RectF> m_pRtData;
  Mask<CFWL_PartState> m_dwStates = CFWL_PartState::kNormal;
  Part m_iPart = Part::kNone;
  bool m_bMaximize = false;
  bool m_bStaticBackground = false;

 private:
  UnownedPtr<CFWL_Widget> const m_pWidget;
};

#endif  // XFA_FWL_CFWL_THEMEPART_H_
