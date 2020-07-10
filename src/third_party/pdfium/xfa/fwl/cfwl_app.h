// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_APP_H_
#define XFA_FWL_CFWL_APP_H_

#include <memory>

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/timerhandler_iface.h"
#include "xfa/fwl/cfwl_widgetmgr.h"

class CFWL_NoteDriver;
class CFWL_WidgetMgr;

enum FWL_KeyFlag {
  FWL_KEYFLAG_Ctrl = 1 << 0,
  FWL_KEYFLAG_Alt = 1 << 1,
  FWL_KEYFLAG_Shift = 1 << 2,
  FWL_KEYFLAG_Command = 1 << 3,
  FWL_KEYFLAG_LButton = 1 << 4,
  FWL_KEYFLAG_RButton = 1 << 5,
  FWL_KEYFLAG_MButton = 1 << 6
};

class CFWL_App {
 public:
  class AdapterIface {
   public:
    virtual ~AdapterIface() = default;
    virtual CFWL_WidgetMgr::AdapterIface* GetWidgetMgrAdapter() = 0;
    virtual TimerHandlerIface* GetTimerHandler() = 0;
  };

  explicit CFWL_App(AdapterIface* pAdapter);
  ~CFWL_App();

  AdapterIface* GetAdapterNative() const { return m_pAdapterNative.Get(); }
  CFWL_WidgetMgr* GetWidgetMgr() const { return m_pWidgetMgr.get(); }
  CFWL_NoteDriver* GetNoteDriver() const { return m_pNoteDriver.get(); }

 private:
  UnownedPtr<AdapterIface> const m_pAdapterNative;
  std::unique_ptr<CFWL_WidgetMgr> m_pWidgetMgr;
  std::unique_ptr<CFWL_NoteDriver> m_pNoteDriver;
};

#endif  // XFA_FWL_CFWL_APP_H_
