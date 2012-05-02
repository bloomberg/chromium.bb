// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ELEVATED_CONTROLLER_WIN_H_
#define REMOTING_HOST_ELEVATED_CONTROLLER_WIN_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

#include "remoting/base/scoped_sc_handle_win.h"

// MIDL-generated declarations.
#include "remoting/host/elevated_controller.h"

namespace remoting {

class ATL_NO_VTABLE ElevatedControllerWin
    : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
      public ATL::CComCoClass<ElevatedControllerWin, &CLSID_ElevatedController>,
      public ATL::IDispatchImpl<IDaemonControlUi, &IID_IDaemonControlUi,
                                &LIBID_ChromotingElevatedControllerLib, 1, 0> {
 public:
  ElevatedControllerWin();

  HRESULT FinalConstruct();
  void FinalRelease();

  // IDaemonControlUi implementation.
  STDMETHOD(GetConfig)(BSTR* config);
  STDMETHOD(SetConfig)(BSTR config);
  STDMETHOD(StartDaemon)();
  STDMETHOD(StopDaemon)();
  STDMETHOD(UpdateConfig)(BSTR config);
  STDMETHOD(SetOwnerWindow)(LONG_PTR owner_window);

  DECLARE_NO_REGISTRY()

 private:
  HRESULT OpenService(ScopedScHandle* service_out);

  BEGIN_COM_MAP(ElevatedControllerWin)
    COM_INTERFACE_ENTRY(IDaemonControl)
    COM_INTERFACE_ENTRY(IDaemonControlUi)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

  // Handle of the owner window (if any) for any UI to be shown.
  HWND owner_window_;

  DECLARE_PROTECT_FINAL_CONSTRUCT()
};

OBJECT_ENTRY_AUTO(CLSID_ElevatedController, ElevatedControllerWin)

} // namespace remoting

#endif // REMOTING_HOST_ELEVATED_CONTROLLER_WIN_H_
