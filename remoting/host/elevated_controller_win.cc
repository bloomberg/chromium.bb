// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/elevated_controller_win.h"

// MIDL-generated definitions.
#include <elevated_controller_i.c>

using ATL::CComQIPtr;
using ATL::CComPtr;

namespace remoting {

ElevatedControllerWin::ElevatedControllerWin() {
}

HRESULT ElevatedControllerWin::FinalConstruct() {
  return S_OK;
}

void ElevatedControllerWin::FinalRelease() {
}

STDMETHODIMP ElevatedControllerWin::get_State(DaemonState* state_out) {
  return E_NOTIMPL;
}

STDMETHODIMP ElevatedControllerWin::ReadConfig(BSTR* config_out) {
  return E_NOTIMPL;
}

STDMETHODIMP ElevatedControllerWin::WriteConfig(BSTR config) {
  return E_NOTIMPL;
}

STDMETHODIMP ElevatedControllerWin::StartDaemon() {
  return E_NOTIMPL;
}

STDMETHODIMP ElevatedControllerWin::StopDaemon() {
  return E_NOTIMPL;
}

HRESULT ElevatedControllerWin::FireOnStateChange(DaemonState state) {
  CComPtr<IConnectionPoint> connection_point;
  FindConnectionPoint(__uuidof(IDaemonEvents), &connection_point);
  if (!connection_point) {
    return S_OK;
  }

  CComPtr<IEnumConnections> connections;
  if (FAILED(connection_point->EnumConnections(&connections))) {
    return S_OK;
  }

  CONNECTDATA connect_data;
  while (connections->Next(1, &connect_data, NULL) == S_OK) {
    if (connect_data.pUnk != NULL) {
      CComQIPtr<IDaemonEvents, &__uuidof(IDaemonEvents)> sink(
          connect_data.pUnk);

      if (sink != NULL) {
        sink->OnStateChange(state);
      }

      connect_data.pUnk->Release();
    }
  }

  return S_OK;
}

} // namespace remoting
