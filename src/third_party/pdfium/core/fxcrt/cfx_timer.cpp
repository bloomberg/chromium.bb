// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/cfx_timer.h"

#include <map>

namespace {

std::map<int32_t, CFX_Timer*>& GetPWLTimeMap() {
  // Leak the object at shutdown.
  static auto* timeMap = new std::map<int32_t, CFX_Timer*>;
  return *timeMap;
}

}  // namespace

CFX_Timer::CFX_Timer(TimerHandlerIface* pTimerHandler,
                     CallbackIface* pCallbackIface,
                     int32_t nInterval)
    : m_nTimerID(pTimerHandler->SetTimer(nInterval, TimerProc)),
      m_pTimerHandler(pTimerHandler),
      m_pCallbackIface(pCallbackIface) {
  ASSERT(m_pCallbackIface);
  if (HasValidID())
    GetPWLTimeMap()[m_nTimerID] = this;
}

CFX_Timer::~CFX_Timer() {
  if (HasValidID()) {
    m_pTimerHandler->KillTimer(m_nTimerID);
    GetPWLTimeMap().erase(m_nTimerID);
  }
}

// static
void CFX_Timer::TimerProc(int32_t idEvent) {
  auto it = GetPWLTimeMap().find(idEvent);
  if (it != GetPWLTimeMap().end())
    it->second->m_pCallbackIface->OnTimerFired();
}
