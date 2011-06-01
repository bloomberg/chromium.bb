// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_THUNK_H_
#define PPAPI_THUNK_THUNK_H_

struct PPB_Audio;
struct PPB_AudioConfig;
struct PPB_AudioTrusted;
struct PPB_BrokerTrusted;
struct PPB_Buffer_Dev;
struct PPB_CharSet_Dev;
struct PPB_CursorControl_Dev;
struct PPB_Font_Dev;
struct PPB_Graphics2D;
struct PPB_ImageData;

namespace ppapi {
namespace thunk {

const PPB_Audio* GetPPB_Audio_Thunk();
const PPB_AudioConfig* GetPPB_AudioConfig_Thunk();
const PPB_AudioTrusted* GetPPB_AudioTrusted_Thunk();
const PPB_BrokerTrusted* GetPPB_Broker_Thunk();
const PPB_Buffer_Dev* GetPPB_Buffer_Thunk();
const PPB_CharSet_Dev* GetPPB_CharSet_Thunk();
const PPB_CursorControl_Dev* GetPPB_CursorControl_Thunk();
const PPB_Font_Dev* GetPPB_Font_Thunk();
const PPB_Graphics2D* GetPPB_Graphics2D_Thunk();
const PPB_ImageData* GetPPB_ImageData_Thunk();

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_THUNK_H_


