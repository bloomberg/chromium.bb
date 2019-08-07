// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fgas/layout/cfx_linkuserdata.h"

CFX_LinkUserData::CFX_LinkUserData(const wchar_t* pszText)
    : m_wsURLContent(pszText) {}

CFX_LinkUserData::~CFX_LinkUserData() {}
