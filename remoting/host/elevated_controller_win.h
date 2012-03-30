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
#include "elevated_controller.h"

namespace remoting {

class ATL_NO_VTABLE ElevatedControllerWin
    : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
      public ATL::CComCoClass<ElevatedControllerWin, &CLSID_ElevatedController>,
      public ATL::IDispatchImpl<IDaemonControl, &IID_IDaemonControl,
                                &LIBID_ChromotingElevatedControllerLib, 1, 0> {
 public:
  ElevatedControllerWin();

  HRESULT FinalConstruct();
  void FinalRelease();

  // IDaemonControl implementation.
  STDMETHOD(GetConfig)(BSTR* config_out);
  STDMETHOD(SetConfig)(BSTR config);
  STDMETHOD(StartDaemon)();
  STDMETHOD(StopDaemon)();

  DECLARE_NO_REGISTRY()

 private:
  HRESULT OpenService(ScopedScHandle* service_out);

  BEGIN_COM_MAP(ElevatedControllerWin)
    COM_INTERFACE_ENTRY(IDaemonControl)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()
};

OBJECT_ENTRY_AUTO(CLSID_ElevatedController, ElevatedControllerWin)

} // namespace remoting

#endif // REMOTING_HOST_ELEVATED_CONTROLLER_WIN_H_
