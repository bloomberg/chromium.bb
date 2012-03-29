// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ELEVATED_CONTROLLER_WIN_H_
#define REMOTING_HOST_ELEVATED_CONTROLLER_WIN_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

// MIDL-generated declarations.
#include <elevated_controller.h>

namespace remoting {

class ATL_NO_VTABLE ElevatedControllerWin
    : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
      public ATL::CComCoClass<ElevatedControllerWin, &CLSID_ElevatedController>,
      public ATL::IDispatchImpl<IDaemonControl, &IID_IDaemonControl,
                                &LIBID_ChromotingElevatedControllerLib, 1, 0>,
      public ATL::IConnectionPointContainerImpl<ElevatedControllerWin>,
      public ATL::IConnectionPointImpl<ElevatedControllerWin,
                                       &IID_IDaemonEvents> {
 public:
  ElevatedControllerWin();

  HRESULT FinalConstruct();
  void FinalRelease();

  // IDaemonControl implementation.
  STDMETHOD(get_State)(DaemonState* state_out);
  STDMETHOD(ReadConfig)(BSTR* config_out);
  STDMETHOD(WriteConfig)(BSTR config);
  STDMETHOD(StartDaemon)();
  STDMETHOD(StopDaemon)();

  // Fires IDaemonEvents::OnStateChange notification.
  HRESULT FireOnStateChange(DaemonState state);

  DECLARE_NO_REGISTRY()

  BEGIN_COM_MAP(ElevatedControllerWin)
    COM_INTERFACE_ENTRY(IDaemonControl)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IConnectionPointContainer)
  END_COM_MAP()

  BEGIN_CONNECTION_POINT_MAP(ElevatedControllerWin)
    CONNECTION_POINT_ENTRY(IID_IDaemonEvents)
  END_CONNECTION_POINT_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()
};

OBJECT_ENTRY_AUTO(CLSID_ElevatedController, ElevatedControllerWin)

} // namespace remoting

#endif // REMOTING_HOST_ELEVATED_CONTROLLER_WIN_H_
