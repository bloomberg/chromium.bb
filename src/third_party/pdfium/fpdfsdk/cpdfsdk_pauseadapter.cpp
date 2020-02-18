// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_pauseadapter.h"

CPDFSDK_PauseAdapter::CPDFSDK_PauseAdapter(IFSDK_PAUSE* IPause)
    : m_IPause(IPause) {}

CPDFSDK_PauseAdapter::~CPDFSDK_PauseAdapter() = default;

bool CPDFSDK_PauseAdapter::NeedToPauseNow() {
  return m_IPause->NeedToPauseNow && m_IPause->NeedToPauseNow(m_IPause.Get());
}
