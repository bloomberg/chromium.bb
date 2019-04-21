/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the w64 mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error This stub requires an updated version of <rpcndr.h>
#endif

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif

#ifndef __ocidl_h__
#define __ocidl_h__

#ifndef __IEnumConnections_FWD_DEFINED__
#define __IEnumConnections_FWD_DEFINED__
typedef struct IEnumConnections IEnumConnections;
#endif

#ifndef __IConnectionPoint_FWD_DEFINED__
#define __IConnectionPoint_FWD_DEFINED__
typedef struct IConnectionPoint IConnectionPoint;
#endif

#ifndef __IEnumConnectionPoints_FWD_DEFINED__
#define __IEnumConnectionPoints_FWD_DEFINED__
typedef struct IEnumConnectionPoints IEnumConnectionPoints;
#endif

#ifndef __IConnectionPointContainer_FWD_DEFINED__
#define __IConnectionPointContainer_FWD_DEFINED__
typedef struct IConnectionPointContainer IConnectionPointContainer;
#endif

#ifndef __IClassFactory2_FWD_DEFINED__
#define __IClassFactory2_FWD_DEFINED__
typedef struct IClassFactory2 IClassFactory2;
#endif

#ifndef __IProvideClassInfo_FWD_DEFINED__
#define __IProvideClassInfo_FWD_DEFINED__
typedef struct IProvideClassInfo IProvideClassInfo;
#endif

#ifndef __IProvideClassInfo2_FWD_DEFINED__
#define __IProvideClassInfo2_FWD_DEFINED__
typedef struct IProvideClassInfo2 IProvideClassInfo2;
#endif

#ifndef __IProvideMultipleClassInfo_FWD_DEFINED__
#define __IProvideMultipleClassInfo_FWD_DEFINED__
typedef struct IProvideMultipleClassInfo IProvideMultipleClassInfo;
#endif

#ifndef __IOleControl_FWD_DEFINED__
#define __IOleControl_FWD_DEFINED__
typedef struct IOleControl IOleControl;
#endif

#ifndef __IOleControlSite_FWD_DEFINED__
#define __IOleControlSite_FWD_DEFINED__
typedef struct IOleControlSite IOleControlSite;
#endif

#ifndef __IPropertyPage_FWD_DEFINED__
#define __IPropertyPage_FWD_DEFINED__
typedef struct IPropertyPage IPropertyPage;
#endif

#ifndef __IPropertyPage2_FWD_DEFINED__
#define __IPropertyPage2_FWD_DEFINED__
typedef struct IPropertyPage2 IPropertyPage2;
#endif

#ifndef __IPropertyPageSite_FWD_DEFINED__
#define __IPropertyPageSite_FWD_DEFINED__
typedef struct IPropertyPageSite IPropertyPageSite;
#endif

#ifndef __IPropertyNotifySink_FWD_DEFINED__
#define __IPropertyNotifySink_FWD_DEFINED__
typedef struct IPropertyNotifySink IPropertyNotifySink;
#endif

#ifndef __ISpecifyPropertyPages_FWD_DEFINED__
#define __ISpecifyPropertyPages_FWD_DEFINED__
typedef struct ISpecifyPropertyPages ISpecifyPropertyPages;
#endif

#ifndef __IPersistMemory_FWD_DEFINED__
#define __IPersistMemory_FWD_DEFINED__
typedef struct IPersistMemory IPersistMemory;
#endif

#ifndef __IPersistStreamInit_FWD_DEFINED__
#define __IPersistStreamInit_FWD_DEFINED__
typedef struct IPersistStreamInit IPersistStreamInit;
#endif

#ifndef __IPersistPropertyBag_FWD_DEFINED__
#define __IPersistPropertyBag_FWD_DEFINED__
typedef struct IPersistPropertyBag IPersistPropertyBag;
#endif

#ifndef __ISimpleFrameSite_FWD_DEFINED__
#define __ISimpleFrameSite_FWD_DEFINED__
typedef struct ISimpleFrameSite ISimpleFrameSite;
#endif

#ifndef __IFont_FWD_DEFINED__
#define __IFont_FWD_DEFINED__
typedef struct IFont IFont;
#endif

#ifndef __IPicture_FWD_DEFINED__
#define __IPicture_FWD_DEFINED__
typedef struct IPicture IPicture;
#endif

#ifndef __IFontEventsDisp_FWD_DEFINED__
#define __IFontEventsDisp_FWD_DEFINED__
typedef struct IFontEventsDisp IFontEventsDisp;
#endif

#ifndef __IFontDisp_FWD_DEFINED__
#define __IFontDisp_FWD_DEFINED__
typedef struct IFontDisp IFontDisp;
#endif

#ifndef __IPictureDisp_FWD_DEFINED__
#define __IPictureDisp_FWD_DEFINED__
typedef struct IPictureDisp IPictureDisp;
#endif

#ifndef __IOleInPlaceObjectWindowless_FWD_DEFINED__
#define __IOleInPlaceObjectWindowless_FWD_DEFINED__
typedef struct IOleInPlaceObjectWindowless IOleInPlaceObjectWindowless;
#endif

#ifndef __IOleInPlaceSiteEx_FWD_DEFINED__
#define __IOleInPlaceSiteEx_FWD_DEFINED__
typedef struct IOleInPlaceSiteEx IOleInPlaceSiteEx;
#endif

#ifndef __IOleInPlaceSiteWindowless_FWD_DEFINED__
#define __IOleInPlaceSiteWindowless_FWD_DEFINED__
typedef struct IOleInPlaceSiteWindowless IOleInPlaceSiteWindowless;
#endif

#ifndef __IViewObjectEx_FWD_DEFINED__
#define __IViewObjectEx_FWD_DEFINED__
typedef struct IViewObjectEx IViewObjectEx;
#endif

#ifndef __IOleUndoUnit_FWD_DEFINED__
#define __IOleUndoUnit_FWD_DEFINED__
typedef struct IOleUndoUnit IOleUndoUnit;
#endif

#ifndef __IOleParentUndoUnit_FWD_DEFINED__
#define __IOleParentUndoUnit_FWD_DEFINED__
typedef struct IOleParentUndoUnit IOleParentUndoUnit;
#endif

#ifndef __IEnumOleUndoUnits_FWD_DEFINED__
#define __IEnumOleUndoUnits_FWD_DEFINED__
typedef struct IEnumOleUndoUnits IEnumOleUndoUnits;
#endif

#ifndef __IOleUndoManager_FWD_DEFINED__
#define __IOleUndoManager_FWD_DEFINED__
typedef struct IOleUndoManager IOleUndoManager;
#endif

#ifndef __IPointerInactive_FWD_DEFINED__
#define __IPointerInactive_FWD_DEFINED__
typedef struct IPointerInactive IPointerInactive;
#endif

#ifndef __IObjectWithSite_FWD_DEFINED__
#define __IObjectWithSite_FWD_DEFINED__
typedef struct IObjectWithSite IObjectWithSite;
#endif

#ifndef __IPerPropertyBrowsing_FWD_DEFINED__
#define __IPerPropertyBrowsing_FWD_DEFINED__
typedef struct IPerPropertyBrowsing IPerPropertyBrowsing;
#endif

#ifndef __IPropertyBag2_FWD_DEFINED__
#define __IPropertyBag2_FWD_DEFINED__
typedef struct IPropertyBag2 IPropertyBag2;
#endif

#ifndef __IPersistPropertyBag2_FWD_DEFINED__
#define __IPersistPropertyBag2_FWD_DEFINED__
typedef struct IPersistPropertyBag2 IPersistPropertyBag2;
#endif

#ifndef __IAdviseSinkEx_FWD_DEFINED__
#define __IAdviseSinkEx_FWD_DEFINED__
typedef struct IAdviseSinkEx IAdviseSinkEx;
#endif

#ifndef __IQuickActivate_FWD_DEFINED__
#define __IQuickActivate_FWD_DEFINED__
typedef struct IQuickActivate IQuickActivate;
#endif

#include "oleidl.h"
#include "oaidl.h"
#include "servprov.h"
#include "urlmon.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __MIDL_user_allocate_free_DEFINED__
#define __MIDL_user_allocate_free_DEFINED__
  void *__RPC_API MIDL_user_allocate(size_t);
  void __RPC_API MIDL_user_free(void *);
#endif

  extern RPC_IF_HANDLE __MIDL_itf_ocidl_0000_v0_0_c_ifspec;
  extern RPC_IF_HANDLE __MIDL_itf_ocidl_0000_v0_0_s_ifspec;
#ifndef __IOleControlTypes_INTERFACE_DEFINED__
#define __IOleControlTypes_INTERFACE_DEFINED__
  typedef enum tagUASFLAGS {
    UAS_NORMAL = 0,UAS_BLOCKED = 0x1,UAS_NOPARENTENABLE = 0x2,UAS_MASK = 0x3
  } UASFLAGS;

  typedef enum tagREADYSTATE {
    READYSTATE_UNINITIALIZED = 0,READYSTATE_LOADING = 1,READYSTATE_LOADED = 2,READYSTATE_INTERACTIVE = 3,READYSTATE_COMPLETE = 4
  } READYSTATE;

  extern RPC_IF_HANDLE IOleControlTypes_v1_0_c_ifspec;
  extern RPC_IF_HANDLE IOleControlTypes_v1_0_s_ifspec;
#endif

#ifndef __IEnumConnections_INTERFACE_DEFINED__
#define __IEnumConnections_INTERFACE_DEFINED__
  typedef IEnumConnections *PENUMCONNECTIONS;
  typedef IEnumConnections *LPENUMCONNECTIONS;

  typedef struct tagCONNECTDATA {
    IUnknown *pUnk;
    DWORD dwCookie;
  } CONNECTDATA;

  typedef struct tagCONNECTDATA *PCONNECTDATA;
  typedef struct tagCONNECTDATA *LPCONNECTDATA;

  EXTERN_C const IID IID_IEnumConnections;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IEnumConnections : public IUnknown {
  public:
    virtual HRESULT WINAPI Next(ULONG cConnections,LPCONNECTDATA rgcd,ULONG *pcFetched) = 0;
    virtual HRESULT WINAPI Skip(ULONG cConnections) = 0;
    virtual HRESULT WINAPI Reset(void) = 0;
    virtual HRESULT WINAPI Clone(IEnumConnections **ppEnum) = 0;
  };
#else
  typedef struct IEnumConnectionsVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IEnumConnections *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IEnumConnections *This);
      ULONG (WINAPI *Release)(IEnumConnections *This);
      HRESULT (WINAPI *Next)(IEnumConnections *This,ULONG cConnections,LPCONNECTDATA rgcd,ULONG *pcFetched);
      HRESULT (WINAPI *Skip)(IEnumConnections *This,ULONG cConnections);
      HRESULT (WINAPI *Reset)(IEnumConnections *This);
      HRESULT (WINAPI *Clone)(IEnumConnections *This,IEnumConnections **ppEnum);
    END_INTERFACE
  } IEnumConnectionsVtbl;
  struct IEnumConnections {
    CONST_VTBL struct IEnumConnectionsVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IEnumConnections_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IEnumConnections_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IEnumConnections_Release(This) (This)->lpVtbl->Release(This)
#define IEnumConnections_Next(This,cConnections,rgcd,pcFetched) (This)->lpVtbl->Next(This,cConnections,rgcd,pcFetched)
#define IEnumConnections_Skip(This,cConnections) (This)->lpVtbl->Skip(This,cConnections)
#define IEnumConnections_Reset(This) (This)->lpVtbl->Reset(This)
#define IEnumConnections_Clone(This,ppEnum) (This)->lpVtbl->Clone(This,ppEnum)
#endif
#endif
  HRESULT WINAPI IEnumConnections_RemoteNext_Proxy(IEnumConnections *This,ULONG cConnections,LPCONNECTDATA rgcd,ULONG *pcFetched);
  void __RPC_STUB IEnumConnections_RemoteNext_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IEnumConnections_Skip_Proxy(IEnumConnections *This,ULONG cConnections);
  void __RPC_STUB IEnumConnections_Skip_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IEnumConnections_Reset_Proxy(IEnumConnections *This);
  void __RPC_STUB IEnumConnections_Reset_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IEnumConnections_Clone_Proxy(IEnumConnections *This,IEnumConnections **ppEnum);
  void __RPC_STUB IEnumConnections_Clone_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IConnectionPoint_INTERFACE_DEFINED__
#define __IConnectionPoint_INTERFACE_DEFINED__
  typedef IConnectionPoint *PCONNECTIONPOINT;
  typedef IConnectionPoint *LPCONNECTIONPOINT;

  EXTERN_C const IID IID_IConnectionPoint;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IConnectionPoint : public IUnknown {
  public:
    virtual HRESULT WINAPI GetConnectionInterface(IID *pIID) = 0;
    virtual HRESULT WINAPI GetConnectionPointContainer(IConnectionPointContainer **ppCPC) = 0;
    virtual HRESULT WINAPI Advise(IUnknown *pUnkSink,DWORD *pdwCookie) = 0;
    virtual HRESULT WINAPI Unadvise(DWORD dwCookie) = 0;
    virtual HRESULT WINAPI EnumConnections(IEnumConnections **ppEnum) = 0;
  };
#else
  typedef struct IConnectionPointVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IConnectionPoint *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IConnectionPoint *This);
      ULONG (WINAPI *Release)(IConnectionPoint *This);
      HRESULT (WINAPI *GetConnectionInterface)(IConnectionPoint *This,IID *pIID);
      HRESULT (WINAPI *GetConnectionPointContainer)(IConnectionPoint *This,IConnectionPointContainer **ppCPC);
      HRESULT (WINAPI *Advise)(IConnectionPoint *This,IUnknown *pUnkSink,DWORD *pdwCookie);
      HRESULT (WINAPI *Unadvise)(IConnectionPoint *This,DWORD dwCookie);
      HRESULT (WINAPI *EnumConnections)(IConnectionPoint *This,IEnumConnections **ppEnum);
    END_INTERFACE
  } IConnectionPointVtbl;
  struct IConnectionPoint {
    CONST_VTBL struct IConnectionPointVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IConnectionPoint_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IConnectionPoint_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IConnectionPoint_Release(This) (This)->lpVtbl->Release(This)
#define IConnectionPoint_GetConnectionInterface(This,pIID) (This)->lpVtbl->GetConnectionInterface(This,pIID)
#define IConnectionPoint_GetConnectionPointContainer(This,ppCPC) (This)->lpVtbl->GetConnectionPointContainer(This,ppCPC)
#define IConnectionPoint_Advise(This,pUnkSink,pdwCookie) (This)->lpVtbl->Advise(This,pUnkSink,pdwCookie)
#define IConnectionPoint_Unadvise(This,dwCookie) (This)->lpVtbl->Unadvise(This,dwCookie)
#define IConnectionPoint_EnumConnections(This,ppEnum) (This)->lpVtbl->EnumConnections(This,ppEnum)
#endif
#endif
  HRESULT WINAPI IConnectionPoint_GetConnectionInterface_Proxy(IConnectionPoint *This,IID *pIID);
  void __RPC_STUB IConnectionPoint_GetConnectionInterface_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IConnectionPoint_GetConnectionPointContainer_Proxy(IConnectionPoint *This,IConnectionPointContainer **ppCPC);
  void __RPC_STUB IConnectionPoint_GetConnectionPointContainer_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IConnectionPoint_Advise_Proxy(IConnectionPoint *This,IUnknown *pUnkSink,DWORD *pdwCookie);
  void __RPC_STUB IConnectionPoint_Advise_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IConnectionPoint_Unadvise_Proxy(IConnectionPoint *This,DWORD dwCookie);
  void __RPC_STUB IConnectionPoint_Unadvise_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IConnectionPoint_EnumConnections_Proxy(IConnectionPoint *This,IEnumConnections **ppEnum);
  void __RPC_STUB IConnectionPoint_EnumConnections_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IEnumConnectionPoints_INTERFACE_DEFINED__
#define __IEnumConnectionPoints_INTERFACE_DEFINED__
  typedef IEnumConnectionPoints *PENUMCONNECTIONPOINTS;
  typedef IEnumConnectionPoints *LPENUMCONNECTIONPOINTS;

  EXTERN_C const IID IID_IEnumConnectionPoints;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IEnumConnectionPoints : public IUnknown {
  public:
    virtual HRESULT WINAPI Next(ULONG cConnections,LPCONNECTIONPOINT *ppCP,ULONG *pcFetched) = 0;
    virtual HRESULT WINAPI Skip(ULONG cConnections) = 0;
    virtual HRESULT WINAPI Reset(void) = 0;
    virtual HRESULT WINAPI Clone(IEnumConnectionPoints **ppEnum) = 0;
  };
#else
  typedef struct IEnumConnectionPointsVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IEnumConnectionPoints *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IEnumConnectionPoints *This);
      ULONG (WINAPI *Release)(IEnumConnectionPoints *This);
      HRESULT (WINAPI *Next)(IEnumConnectionPoints *This,ULONG cConnections,LPCONNECTIONPOINT *ppCP,ULONG *pcFetched);
      HRESULT (WINAPI *Skip)(IEnumConnectionPoints *This,ULONG cConnections);
      HRESULT (WINAPI *Reset)(IEnumConnectionPoints *This);
      HRESULT (WINAPI *Clone)(IEnumConnectionPoints *This,IEnumConnectionPoints **ppEnum);
    END_INTERFACE
  } IEnumConnectionPointsVtbl;
  struct IEnumConnectionPoints {
    CONST_VTBL struct IEnumConnectionPointsVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IEnumConnectionPoints_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IEnumConnectionPoints_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IEnumConnectionPoints_Release(This) (This)->lpVtbl->Release(This)
#define IEnumConnectionPoints_Next(This,cConnections,ppCP,pcFetched) (This)->lpVtbl->Next(This,cConnections,ppCP,pcFetched)
#define IEnumConnectionPoints_Skip(This,cConnections) (This)->lpVtbl->Skip(This,cConnections)
#define IEnumConnectionPoints_Reset(This) (This)->lpVtbl->Reset(This)
#define IEnumConnectionPoints_Clone(This,ppEnum) (This)->lpVtbl->Clone(This,ppEnum)
#endif
#endif
  HRESULT WINAPI IEnumConnectionPoints_RemoteNext_Proxy(IEnumConnectionPoints *This,ULONG cConnections,LPCONNECTIONPOINT *ppCP,ULONG *pcFetched);
  void __RPC_STUB IEnumConnectionPoints_RemoteNext_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IEnumConnectionPoints_Skip_Proxy(IEnumConnectionPoints *This,ULONG cConnections);
  void __RPC_STUB IEnumConnectionPoints_Skip_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IEnumConnectionPoints_Reset_Proxy(IEnumConnectionPoints *This);
  void __RPC_STUB IEnumConnectionPoints_Reset_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IEnumConnectionPoints_Clone_Proxy(IEnumConnectionPoints *This,IEnumConnectionPoints **ppEnum);
  void __RPC_STUB IEnumConnectionPoints_Clone_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IConnectionPointContainer_INTERFACE_DEFINED__
#define __IConnectionPointContainer_INTERFACE_DEFINED__
  typedef IConnectionPointContainer *PCONNECTIONPOINTCONTAINER;
  typedef IConnectionPointContainer *LPCONNECTIONPOINTCONTAINER;

  EXTERN_C const IID IID_IConnectionPointContainer;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IConnectionPointContainer : public IUnknown {
  public:
    virtual HRESULT WINAPI EnumConnectionPoints(IEnumConnectionPoints **ppEnum) = 0;
    virtual HRESULT WINAPI FindConnectionPoint(REFIID riid,IConnectionPoint **ppCP) = 0;
  };
#else
  typedef struct IConnectionPointContainerVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IConnectionPointContainer *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IConnectionPointContainer *This);
      ULONG (WINAPI *Release)(IConnectionPointContainer *This);
      HRESULT (WINAPI *EnumConnectionPoints)(IConnectionPointContainer *This,IEnumConnectionPoints **ppEnum);
      HRESULT (WINAPI *FindConnectionPoint)(IConnectionPointContainer *This,REFIID riid,IConnectionPoint **ppCP);
    END_INTERFACE
  } IConnectionPointContainerVtbl;
  struct IConnectionPointContainer {
    CONST_VTBL struct IConnectionPointContainerVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IConnectionPointContainer_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IConnectionPointContainer_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IConnectionPointContainer_Release(This) (This)->lpVtbl->Release(This)
#define IConnectionPointContainer_EnumConnectionPoints(This,ppEnum) (This)->lpVtbl->EnumConnectionPoints(This,ppEnum)
#define IConnectionPointContainer_FindConnectionPoint(This,riid,ppCP) (This)->lpVtbl->FindConnectionPoint(This,riid,ppCP)
#endif
#endif
  HRESULT WINAPI IConnectionPointContainer_EnumConnectionPoints_Proxy(IConnectionPointContainer *This,IEnumConnectionPoints **ppEnum);
  void __RPC_STUB IConnectionPointContainer_EnumConnectionPoints_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IConnectionPointContainer_FindConnectionPoint_Proxy(IConnectionPointContainer *This,REFIID riid,IConnectionPoint **ppCP);
  void __RPC_STUB IConnectionPointContainer_FindConnectionPoint_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IClassFactory2_INTERFACE_DEFINED__
#define __IClassFactory2_INTERFACE_DEFINED__
  typedef IClassFactory2 *LPCLASSFACTORY2;

  typedef struct tagLICINFO {
    LONG cbLicInfo;
    WINBOOL fRuntimeKeyAvail;
    WINBOOL fLicVerified;
  } LICINFO;

  typedef struct tagLICINFO *LPLICINFO;

  EXTERN_C const IID IID_IClassFactory2;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IClassFactory2 : public IClassFactory {
  public:
    virtual HRESULT WINAPI GetLicInfo(LICINFO *pLicInfo) = 0;
    virtual HRESULT WINAPI RequestLicKey(DWORD dwReserved,BSTR *pBstrKey) = 0;
    virtual HRESULT WINAPI CreateInstanceLic(IUnknown *pUnkOuter,IUnknown *pUnkReserved,REFIID riid,BSTR bstrKey,PVOID *ppvObj) = 0;
  };
#else
  typedef struct IClassFactory2Vtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IClassFactory2 *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IClassFactory2 *This);
      ULONG (WINAPI *Release)(IClassFactory2 *This);
      HRESULT (WINAPI *CreateInstance)(IClassFactory2 *This,IUnknown *pUnkOuter,REFIID riid,void **ppvObject);
      HRESULT (WINAPI *LockServer)(IClassFactory2 *This,WINBOOL fLock);
      HRESULT (WINAPI *GetLicInfo)(IClassFactory2 *This,LICINFO *pLicInfo);
      HRESULT (WINAPI *RequestLicKey)(IClassFactory2 *This,DWORD dwReserved,BSTR *pBstrKey);
      HRESULT (WINAPI *CreateInstanceLic)(IClassFactory2 *This,IUnknown *pUnkOuter,IUnknown *pUnkReserved,REFIID riid,BSTR bstrKey,PVOID *ppvObj);
    END_INTERFACE
  } IClassFactory2Vtbl;
  struct IClassFactory2 {
    CONST_VTBL struct IClassFactory2Vtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IClassFactory2_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IClassFactory2_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IClassFactory2_Release(This) (This)->lpVtbl->Release(This)
#define IClassFactory2_CreateInstance(This,pUnkOuter,riid,ppvObject) (This)->lpVtbl->CreateInstance(This,pUnkOuter,riid,ppvObject)
#define IClassFactory2_LockServer(This,fLock) (This)->lpVtbl->LockServer(This,fLock)
#define IClassFactory2_GetLicInfo(This,pLicInfo) (This)->lpVtbl->GetLicInfo(This,pLicInfo)
#define IClassFactory2_RequestLicKey(This,dwReserved,pBstrKey) (This)->lpVtbl->RequestLicKey(This,dwReserved,pBstrKey)
#define IClassFactory2_CreateInstanceLic(This,pUnkOuter,pUnkReserved,riid,bstrKey,ppvObj) (This)->lpVtbl->CreateInstanceLic(This,pUnkOuter,pUnkReserved,riid,bstrKey,ppvObj)
#endif
#endif
  HRESULT WINAPI IClassFactory2_GetLicInfo_Proxy(IClassFactory2 *This,LICINFO *pLicInfo);
  void __RPC_STUB IClassFactory2_GetLicInfo_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IClassFactory2_RequestLicKey_Proxy(IClassFactory2 *This,DWORD dwReserved,BSTR *pBstrKey);
  void __RPC_STUB IClassFactory2_RequestLicKey_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IClassFactory2_RemoteCreateInstanceLic_Proxy(IClassFactory2 *This,REFIID riid,BSTR bstrKey,IUnknown **ppvObj);
  void __RPC_STUB IClassFactory2_RemoteCreateInstanceLic_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IProvideClassInfo_INTERFACE_DEFINED__
#define __IProvideClassInfo_INTERFACE_DEFINED__
  typedef IProvideClassInfo *LPPROVIDECLASSINFO;

  EXTERN_C const IID IID_IProvideClassInfo;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IProvideClassInfo : public IUnknown {
  public:
    virtual HRESULT WINAPI GetClassInfo(ITypeInfo **ppTI) = 0;
  };
#else
  typedef struct IProvideClassInfoVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IProvideClassInfo *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IProvideClassInfo *This);
      ULONG (WINAPI *Release)(IProvideClassInfo *This);
      HRESULT (WINAPI *GetClassInfo)(IProvideClassInfo *This,ITypeInfo **ppTI);
    END_INTERFACE
  } IProvideClassInfoVtbl;
  struct IProvideClassInfo {
    CONST_VTBL struct IProvideClassInfoVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IProvideClassInfo_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IProvideClassInfo_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IProvideClassInfo_Release(This) (This)->lpVtbl->Release(This)
#define IProvideClassInfo_GetClassInfo(This,ppTI) (This)->lpVtbl->GetClassInfo(This,ppTI)
#endif
#endif
  HRESULT WINAPI IProvideClassInfo_GetClassInfo_Proxy(IProvideClassInfo *This,ITypeInfo **ppTI);
  void __RPC_STUB IProvideClassInfo_GetClassInfo_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IProvideClassInfo2_INTERFACE_DEFINED__
#define __IProvideClassInfo2_INTERFACE_DEFINED__
  typedef IProvideClassInfo2 *LPPROVIDECLASSINFO2;

  typedef enum tagGUIDKIND {
    GUIDKIND_DEFAULT_SOURCE_DISP_IID = 1
  } GUIDKIND;

  EXTERN_C const IID IID_IProvideClassInfo2;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IProvideClassInfo2 : public IProvideClassInfo {
  public:
    virtual HRESULT WINAPI GetGUID(DWORD dwGuidKind,GUID *pGUID) = 0;
  };
#else
  typedef struct IProvideClassInfo2Vtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IProvideClassInfo2 *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IProvideClassInfo2 *This);
      ULONG (WINAPI *Release)(IProvideClassInfo2 *This);
      HRESULT (WINAPI *GetClassInfo)(IProvideClassInfo2 *This,ITypeInfo **ppTI);
      HRESULT (WINAPI *GetGUID)(IProvideClassInfo2 *This,DWORD dwGuidKind,GUID *pGUID);
    END_INTERFACE
  } IProvideClassInfo2Vtbl;
  struct IProvideClassInfo2 {
    CONST_VTBL struct IProvideClassInfo2Vtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IProvideClassInfo2_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IProvideClassInfo2_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IProvideClassInfo2_Release(This) (This)->lpVtbl->Release(This)
#define IProvideClassInfo2_GetClassInfo(This,ppTI) (This)->lpVtbl->GetClassInfo(This,ppTI)
#define IProvideClassInfo2_GetGUID(This,dwGuidKind,pGUID) (This)->lpVtbl->GetGUID(This,dwGuidKind,pGUID)
#endif
#endif
  HRESULT WINAPI IProvideClassInfo2_GetGUID_Proxy(IProvideClassInfo2 *This,DWORD dwGuidKind,GUID *pGUID);
  void __RPC_STUB IProvideClassInfo2_GetGUID_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IProvideMultipleClassInfo_INTERFACE_DEFINED__
#define __IProvideMultipleClassInfo_INTERFACE_DEFINED__

#define MULTICLASSINFO_GETTYPEINFO 0x00000001
#define MULTICLASSINFO_GETNUMRESERVEDDISPIDS 0x00000002
#define MULTICLASSINFO_GETIIDPRIMARY 0x00000004
#define MULTICLASSINFO_GETIIDSOURCE 0x00000008
#define TIFLAGS_EXTENDDISPATCHONLY 0x00000001
  typedef IProvideMultipleClassInfo *LPPROVIDEMULTIPLECLASSINFO;

  EXTERN_C const IID IID_IProvideMultipleClassInfo;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IProvideMultipleClassInfo : public IProvideClassInfo2 {
  public:
    virtual HRESULT WINAPI GetMultiTypeInfoCount(ULONG *pcti) = 0;
    virtual HRESULT WINAPI GetInfoOfIndex(ULONG iti,DWORD dwFlags,ITypeInfo **pptiCoClass,DWORD *pdwTIFlags,ULONG *pcdispidReserved,IID *piidPrimary,IID *piidSource) = 0;
  };
#else
  typedef struct IProvideMultipleClassInfoVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IProvideMultipleClassInfo *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IProvideMultipleClassInfo *This);
      ULONG (WINAPI *Release)(IProvideMultipleClassInfo *This);
      HRESULT (WINAPI *GetClassInfo)(IProvideMultipleClassInfo *This,ITypeInfo **ppTI);
      HRESULT (WINAPI *GetGUID)(IProvideMultipleClassInfo *This,DWORD dwGuidKind,GUID *pGUID);
      HRESULT (WINAPI *GetMultiTypeInfoCount)(IProvideMultipleClassInfo *This,ULONG *pcti);
      HRESULT (WINAPI *GetInfoOfIndex)(IProvideMultipleClassInfo *This,ULONG iti,DWORD dwFlags,ITypeInfo **pptiCoClass,DWORD *pdwTIFlags,ULONG *pcdispidReserved,IID *piidPrimary,IID *piidSource);
    END_INTERFACE
  } IProvideMultipleClassInfoVtbl;
  struct IProvideMultipleClassInfo {
    CONST_VTBL struct IProvideMultipleClassInfoVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IProvideMultipleClassInfo_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IProvideMultipleClassInfo_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IProvideMultipleClassInfo_Release(This) (This)->lpVtbl->Release(This)
#define IProvideMultipleClassInfo_GetClassInfo(This,ppTI) (This)->lpVtbl->GetClassInfo(This,ppTI)
#define IProvideMultipleClassInfo_GetGUID(This,dwGuidKind,pGUID) (This)->lpVtbl->GetGUID(This,dwGuidKind,pGUID)
#define IProvideMultipleClassInfo_GetMultiTypeInfoCount(This,pcti) (This)->lpVtbl->GetMultiTypeInfoCount(This,pcti)
#define IProvideMultipleClassInfo_GetInfoOfIndex(This,iti,dwFlags,pptiCoClass,pdwTIFlags,pcdispidReserved,piidPrimary,piidSource) (This)->lpVtbl->GetInfoOfIndex(This,iti,dwFlags,pptiCoClass,pdwTIFlags,pcdispidReserved,piidPrimary,piidSource)
#endif
#endif
  HRESULT WINAPI IProvideMultipleClassInfo_GetMultiTypeInfoCount_Proxy(IProvideMultipleClassInfo *This,ULONG *pcti);
  void __RPC_STUB IProvideMultipleClassInfo_GetMultiTypeInfoCount_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IProvideMultipleClassInfo_GetInfoOfIndex_Proxy(IProvideMultipleClassInfo *This,ULONG iti,DWORD dwFlags,ITypeInfo **pptiCoClass,DWORD *pdwTIFlags,ULONG *pcdispidReserved,IID *piidPrimary,IID *piidSource);
  void __RPC_STUB IProvideMultipleClassInfo_GetInfoOfIndex_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IOleControl_INTERFACE_DEFINED__
#define __IOleControl_INTERFACE_DEFINED__
  typedef IOleControl *LPOLECONTROL;

  typedef struct tagCONTROLINFO {
    ULONG cb;
    HACCEL hAccel;
    USHORT cAccel;
    DWORD dwFlags;
  } CONTROLINFO;

  typedef struct tagCONTROLINFO *LPCONTROLINFO;
  typedef enum tagCTRLINFO {
    CTRLINFO_EATS_RETURN = 1,CTRLINFO_EATS_ESCAPE = 2
  } CTRLINFO;

  EXTERN_C const IID IID_IOleControl;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IOleControl : public IUnknown {
  public:
    virtual HRESULT WINAPI GetControlInfo(CONTROLINFO *pCI) = 0;
    virtual HRESULT WINAPI OnMnemonic(MSG *pMsg) = 0;
    virtual HRESULT WINAPI OnAmbientPropertyChange(DISPID dispID) = 0;
    virtual HRESULT WINAPI FreezeEvents(WINBOOL bFreeze) = 0;
  };
#else
  typedef struct IOleControlVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IOleControl *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IOleControl *This);
      ULONG (WINAPI *Release)(IOleControl *This);
      HRESULT (WINAPI *GetControlInfo)(IOleControl *This,CONTROLINFO *pCI);
      HRESULT (WINAPI *OnMnemonic)(IOleControl *This,MSG *pMsg);
      HRESULT (WINAPI *OnAmbientPropertyChange)(IOleControl *This,DISPID dispID);
      HRESULT (WINAPI *FreezeEvents)(IOleControl *This,WINBOOL bFreeze);
    END_INTERFACE
  } IOleControlVtbl;
  struct IOleControl {
    CONST_VTBL struct IOleControlVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IOleControl_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IOleControl_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IOleControl_Release(This) (This)->lpVtbl->Release(This)
#define IOleControl_GetControlInfo(This,pCI) (This)->lpVtbl->GetControlInfo(This,pCI)
#define IOleControl_OnMnemonic(This,pMsg) (This)->lpVtbl->OnMnemonic(This,pMsg)
#define IOleControl_OnAmbientPropertyChange(This,dispID) (This)->lpVtbl->OnAmbientPropertyChange(This,dispID)
#define IOleControl_FreezeEvents(This,bFreeze) (This)->lpVtbl->FreezeEvents(This,bFreeze)
#endif
#endif
  HRESULT WINAPI IOleControl_GetControlInfo_Proxy(IOleControl *This,CONTROLINFO *pCI);
  void __RPC_STUB IOleControl_GetControlInfo_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleControl_OnMnemonic_Proxy(IOleControl *This,MSG *pMsg);
  void __RPC_STUB IOleControl_OnMnemonic_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleControl_OnAmbientPropertyChange_Proxy(IOleControl *This,DISPID dispID);
  void __RPC_STUB IOleControl_OnAmbientPropertyChange_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleControl_FreezeEvents_Proxy(IOleControl *This,WINBOOL bFreeze);
  void __RPC_STUB IOleControl_FreezeEvents_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IOleControlSite_INTERFACE_DEFINED__
#define __IOleControlSite_INTERFACE_DEFINED__
  typedef IOleControlSite *LPOLECONTROLSITE;

  typedef struct tagPOINTF {
    FLOAT x;
    FLOAT y;
  } POINTF;

  typedef struct tagPOINTF *LPPOINTF;

  typedef enum tagXFORMCOORDS {
    XFORMCOORDS_POSITION = 0x1,XFORMCOORDS_SIZE = 0x2,XFORMCOORDS_HIMETRICTOCONTAINER = 0x4,XFORMCOORDS_CONTAINERTOHIMETRIC = 0x8,
    XFORMCOORDS_EVENTCOMPAT = 0x10
  } XFORMCOORDS;

  EXTERN_C const IID IID_IOleControlSite;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IOleControlSite : public IUnknown {
  public:
    virtual HRESULT WINAPI OnControlInfoChanged(void) = 0;
    virtual HRESULT WINAPI LockInPlaceActive(WINBOOL fLock) = 0;
    virtual HRESULT WINAPI GetExtendedControl(IDispatch **ppDisp) = 0;
    virtual HRESULT WINAPI TransformCoords(POINTL *pPtlHimetric,POINTF *pPtfContainer,DWORD dwFlags) = 0;
    virtual HRESULT WINAPI TranslateAccelerator(MSG *pMsg,DWORD grfModifiers) = 0;
    virtual HRESULT WINAPI OnFocus(WINBOOL fGotFocus) = 0;
    virtual HRESULT WINAPI ShowPropertyFrame(void) = 0;
  };
#else
  typedef struct IOleControlSiteVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IOleControlSite *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IOleControlSite *This);
      ULONG (WINAPI *Release)(IOleControlSite *This);
      HRESULT (WINAPI *OnControlInfoChanged)(IOleControlSite *This);
      HRESULT (WINAPI *LockInPlaceActive)(IOleControlSite *This,WINBOOL fLock);
      HRESULT (WINAPI *GetExtendedControl)(IOleControlSite *This,IDispatch **ppDisp);
      HRESULT (WINAPI *TransformCoords)(IOleControlSite *This,POINTL *pPtlHimetric,POINTF *pPtfContainer,DWORD dwFlags);
      HRESULT (WINAPI *TranslateAccelerator)(IOleControlSite *This,MSG *pMsg,DWORD grfModifiers);
      HRESULT (WINAPI *OnFocus)(IOleControlSite *This,WINBOOL fGotFocus);
      HRESULT (WINAPI *ShowPropertyFrame)(IOleControlSite *This);
    END_INTERFACE
  } IOleControlSiteVtbl;
  struct IOleControlSite {
    CONST_VTBL struct IOleControlSiteVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IOleControlSite_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IOleControlSite_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IOleControlSite_Release(This) (This)->lpVtbl->Release(This)
#define IOleControlSite_OnControlInfoChanged(This) (This)->lpVtbl->OnControlInfoChanged(This)
#define IOleControlSite_LockInPlaceActive(This,fLock) (This)->lpVtbl->LockInPlaceActive(This,fLock)
#define IOleControlSite_GetExtendedControl(This,ppDisp) (This)->lpVtbl->GetExtendedControl(This,ppDisp)
#define IOleControlSite_TransformCoords(This,pPtlHimetric,pPtfContainer,dwFlags) (This)->lpVtbl->TransformCoords(This,pPtlHimetric,pPtfContainer,dwFlags)
#define IOleControlSite_TranslateAccelerator(This,pMsg,grfModifiers) (This)->lpVtbl->TranslateAccelerator(This,pMsg,grfModifiers)
#define IOleControlSite_OnFocus(This,fGotFocus) (This)->lpVtbl->OnFocus(This,fGotFocus)
#define IOleControlSite_ShowPropertyFrame(This) (This)->lpVtbl->ShowPropertyFrame(This)
#endif
#endif
  HRESULT WINAPI IOleControlSite_OnControlInfoChanged_Proxy(IOleControlSite *This);
  void __RPC_STUB IOleControlSite_OnControlInfoChanged_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleControlSite_LockInPlaceActive_Proxy(IOleControlSite *This,WINBOOL fLock);
  void __RPC_STUB IOleControlSite_LockInPlaceActive_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleControlSite_GetExtendedControl_Proxy(IOleControlSite *This,IDispatch **ppDisp);
  void __RPC_STUB IOleControlSite_GetExtendedControl_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleControlSite_TransformCoords_Proxy(IOleControlSite *This,POINTL *pPtlHimetric,POINTF *pPtfContainer,DWORD dwFlags);
  void __RPC_STUB IOleControlSite_TransformCoords_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleControlSite_TranslateAccelerator_Proxy(IOleControlSite *This,MSG *pMsg,DWORD grfModifiers);
  void __RPC_STUB IOleControlSite_TranslateAccelerator_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleControlSite_OnFocus_Proxy(IOleControlSite *This,WINBOOL fGotFocus);
  void __RPC_STUB IOleControlSite_OnFocus_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleControlSite_ShowPropertyFrame_Proxy(IOleControlSite *This);
  void __RPC_STUB IOleControlSite_ShowPropertyFrame_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPropertyPage_INTERFACE_DEFINED__
#define __IPropertyPage_INTERFACE_DEFINED__
  typedef IPropertyPage *LPPROPERTYPAGE;

  typedef struct tagPROPPAGEINFO {
    ULONG cb;
    LPOLESTR pszTitle;
    SIZE size;
    LPOLESTR pszDocString;
    LPOLESTR pszHelpFile;
    DWORD dwHelpContext;
  } PROPPAGEINFO;

  typedef struct tagPROPPAGEINFO *LPPROPPAGEINFO;

  EXTERN_C const IID IID_IPropertyPage;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPropertyPage : public IUnknown {
  public:
    virtual HRESULT WINAPI SetPageSite(IPropertyPageSite *pPageSite) = 0;
    virtual HRESULT WINAPI Activate(HWND hWndParent,LPCRECT pRect,WINBOOL bModal) = 0;
    virtual HRESULT WINAPI Deactivate(void) = 0;
    virtual HRESULT WINAPI GetPageInfo(PROPPAGEINFO *pPageInfo) = 0;
    virtual HRESULT WINAPI SetObjects(ULONG cObjects,IUnknown **ppUnk) = 0;
    virtual HRESULT WINAPI Show(UINT nCmdShow) = 0;
    virtual HRESULT WINAPI Move(LPCRECT pRect) = 0;
    virtual HRESULT WINAPI IsPageDirty(void) = 0;
    virtual HRESULT WINAPI Apply(void) = 0;
    virtual HRESULT WINAPI Help(LPCOLESTR pszHelpDir) = 0;
    virtual HRESULT WINAPI TranslateAccelerator(MSG *pMsg) = 0;
  };
#else
  typedef struct IPropertyPageVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPropertyPage *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPropertyPage *This);
      ULONG (WINAPI *Release)(IPropertyPage *This);
      HRESULT (WINAPI *SetPageSite)(IPropertyPage *This,IPropertyPageSite *pPageSite);
      HRESULT (WINAPI *Activate)(IPropertyPage *This,HWND hWndParent,LPCRECT pRect,WINBOOL bModal);
      HRESULT (WINAPI *Deactivate)(IPropertyPage *This);
      HRESULT (WINAPI *GetPageInfo)(IPropertyPage *This,PROPPAGEINFO *pPageInfo);
      HRESULT (WINAPI *SetObjects)(IPropertyPage *This,ULONG cObjects,IUnknown **ppUnk);
      HRESULT (WINAPI *Show)(IPropertyPage *This,UINT nCmdShow);
      HRESULT (WINAPI *Move)(IPropertyPage *This,LPCRECT pRect);
      HRESULT (WINAPI *IsPageDirty)(IPropertyPage *This);
      HRESULT (WINAPI *Apply)(IPropertyPage *This);
      HRESULT (WINAPI *Help)(IPropertyPage *This,LPCOLESTR pszHelpDir);
      HRESULT (WINAPI *TranslateAccelerator)(IPropertyPage *This,MSG *pMsg);
    END_INTERFACE
  } IPropertyPageVtbl;
  struct IPropertyPage {
    CONST_VTBL struct IPropertyPageVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPropertyPage_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPropertyPage_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPropertyPage_Release(This) (This)->lpVtbl->Release(This)
#define IPropertyPage_SetPageSite(This,pPageSite) (This)->lpVtbl->SetPageSite(This,pPageSite)
#define IPropertyPage_Activate(This,hWndParent,pRect,bModal) (This)->lpVtbl->Activate(This,hWndParent,pRect,bModal)
#define IPropertyPage_Deactivate(This) (This)->lpVtbl->Deactivate(This)
#define IPropertyPage_GetPageInfo(This,pPageInfo) (This)->lpVtbl->GetPageInfo(This,pPageInfo)
#define IPropertyPage_SetObjects(This,cObjects,ppUnk) (This)->lpVtbl->SetObjects(This,cObjects,ppUnk)
#define IPropertyPage_Show(This,nCmdShow) (This)->lpVtbl->Show(This,nCmdShow)
#define IPropertyPage_Move(This,pRect) (This)->lpVtbl->Move(This,pRect)
#define IPropertyPage_IsPageDirty(This) (This)->lpVtbl->IsPageDirty(This)
#define IPropertyPage_Apply(This) (This)->lpVtbl->Apply(This)
#define IPropertyPage_Help(This,pszHelpDir) (This)->lpVtbl->Help(This,pszHelpDir)
#define IPropertyPage_TranslateAccelerator(This,pMsg) (This)->lpVtbl->TranslateAccelerator(This,pMsg)
#endif
#endif
  HRESULT WINAPI IPropertyPage_SetPageSite_Proxy(IPropertyPage *This,IPropertyPageSite *pPageSite);
  void __RPC_STUB IPropertyPage_SetPageSite_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_Activate_Proxy(IPropertyPage *This,HWND hWndParent,LPCRECT pRect,WINBOOL bModal);
  void __RPC_STUB IPropertyPage_Activate_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_Deactivate_Proxy(IPropertyPage *This);
  void __RPC_STUB IPropertyPage_Deactivate_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_GetPageInfo_Proxy(IPropertyPage *This,PROPPAGEINFO *pPageInfo);
  void __RPC_STUB IPropertyPage_GetPageInfo_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_SetObjects_Proxy(IPropertyPage *This,ULONG cObjects,IUnknown **ppUnk);
  void __RPC_STUB IPropertyPage_SetObjects_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_Show_Proxy(IPropertyPage *This,UINT nCmdShow);
  void __RPC_STUB IPropertyPage_Show_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_Move_Proxy(IPropertyPage *This,LPCRECT pRect);
  void __RPC_STUB IPropertyPage_Move_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_IsPageDirty_Proxy(IPropertyPage *This);
  void __RPC_STUB IPropertyPage_IsPageDirty_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_Apply_Proxy(IPropertyPage *This);
  void __RPC_STUB IPropertyPage_Apply_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_Help_Proxy(IPropertyPage *This,LPCOLESTR pszHelpDir);
  void __RPC_STUB IPropertyPage_Help_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPage_TranslateAccelerator_Proxy(IPropertyPage *This,MSG *pMsg);
  void __RPC_STUB IPropertyPage_TranslateAccelerator_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPropertyPage2_INTERFACE_DEFINED__
#define __IPropertyPage2_INTERFACE_DEFINED__
  typedef IPropertyPage2 *LPPROPERTYPAGE2;

  EXTERN_C const IID IID_IPropertyPage2;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPropertyPage2 : public IPropertyPage {
  public:
    virtual HRESULT WINAPI EditProperty(DISPID dispID) = 0;
  };
#else
  typedef struct IPropertyPage2Vtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPropertyPage2 *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPropertyPage2 *This);
      ULONG (WINAPI *Release)(IPropertyPage2 *This);
      HRESULT (WINAPI *SetPageSite)(IPropertyPage2 *This,IPropertyPageSite *pPageSite);
      HRESULT (WINAPI *Activate)(IPropertyPage2 *This,HWND hWndParent,LPCRECT pRect,WINBOOL bModal);
      HRESULT (WINAPI *Deactivate)(IPropertyPage2 *This);
      HRESULT (WINAPI *GetPageInfo)(IPropertyPage2 *This,PROPPAGEINFO *pPageInfo);
      HRESULT (WINAPI *SetObjects)(IPropertyPage2 *This,ULONG cObjects,IUnknown **ppUnk);
      HRESULT (WINAPI *Show)(IPropertyPage2 *This,UINT nCmdShow);
      HRESULT (WINAPI *Move)(IPropertyPage2 *This,LPCRECT pRect);
      HRESULT (WINAPI *IsPageDirty)(IPropertyPage2 *This);
      HRESULT (WINAPI *Apply)(IPropertyPage2 *This);
      HRESULT (WINAPI *Help)(IPropertyPage2 *This,LPCOLESTR pszHelpDir);
      HRESULT (WINAPI *TranslateAccelerator)(IPropertyPage2 *This,MSG *pMsg);
      HRESULT (WINAPI *EditProperty)(IPropertyPage2 *This,DISPID dispID);
    END_INTERFACE
  } IPropertyPage2Vtbl;
  struct IPropertyPage2 {
    CONST_VTBL struct IPropertyPage2Vtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPropertyPage2_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPropertyPage2_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPropertyPage2_Release(This) (This)->lpVtbl->Release(This)
#define IPropertyPage2_SetPageSite(This,pPageSite) (This)->lpVtbl->SetPageSite(This,pPageSite)
#define IPropertyPage2_Activate(This,hWndParent,pRect,bModal) (This)->lpVtbl->Activate(This,hWndParent,pRect,bModal)
#define IPropertyPage2_Deactivate(This) (This)->lpVtbl->Deactivate(This)
#define IPropertyPage2_GetPageInfo(This,pPageInfo) (This)->lpVtbl->GetPageInfo(This,pPageInfo)
#define IPropertyPage2_SetObjects(This,cObjects,ppUnk) (This)->lpVtbl->SetObjects(This,cObjects,ppUnk)
#define IPropertyPage2_Show(This,nCmdShow) (This)->lpVtbl->Show(This,nCmdShow)
#define IPropertyPage2_Move(This,pRect) (This)->lpVtbl->Move(This,pRect)
#define IPropertyPage2_IsPageDirty(This) (This)->lpVtbl->IsPageDirty(This)
#define IPropertyPage2_Apply(This) (This)->lpVtbl->Apply(This)
#define IPropertyPage2_Help(This,pszHelpDir) (This)->lpVtbl->Help(This,pszHelpDir)
#define IPropertyPage2_TranslateAccelerator(This,pMsg) (This)->lpVtbl->TranslateAccelerator(This,pMsg)
#define IPropertyPage2_EditProperty(This,dispID) (This)->lpVtbl->EditProperty(This,dispID)
#endif
#endif
  HRESULT WINAPI IPropertyPage2_EditProperty_Proxy(IPropertyPage2 *This,DISPID dispID);
  void __RPC_STUB IPropertyPage2_EditProperty_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPropertyPageSite_INTERFACE_DEFINED__
#define __IPropertyPageSite_INTERFACE_DEFINED__
  typedef IPropertyPageSite *LPPROPERTYPAGESITE;

  typedef enum tagPROPPAGESTATUS {
    PROPPAGESTATUS_DIRTY = 0x1,PROPPAGESTATUS_VALIDATE = 0x2,PROPPAGESTATUS_CLEAN = 0x4
  } PROPPAGESTATUS;

  EXTERN_C const IID IID_IPropertyPageSite;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPropertyPageSite : public IUnknown {
  public:
    virtual HRESULT WINAPI OnStatusChange(DWORD dwFlags) = 0;
    virtual HRESULT WINAPI GetLocaleID(LCID *pLocaleID) = 0;
    virtual HRESULT WINAPI GetPageContainer(IUnknown **ppUnk) = 0;
    virtual HRESULT WINAPI TranslateAccelerator(MSG *pMsg) = 0;
  };
#else
  typedef struct IPropertyPageSiteVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPropertyPageSite *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPropertyPageSite *This);
      ULONG (WINAPI *Release)(IPropertyPageSite *This);
      HRESULT (WINAPI *OnStatusChange)(IPropertyPageSite *This,DWORD dwFlags);
      HRESULT (WINAPI *GetLocaleID)(IPropertyPageSite *This,LCID *pLocaleID);
      HRESULT (WINAPI *GetPageContainer)(IPropertyPageSite *This,IUnknown **ppUnk);
      HRESULT (WINAPI *TranslateAccelerator)(IPropertyPageSite *This,MSG *pMsg);
    END_INTERFACE
  } IPropertyPageSiteVtbl;
  struct IPropertyPageSite {
    CONST_VTBL struct IPropertyPageSiteVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPropertyPageSite_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPropertyPageSite_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPropertyPageSite_Release(This) (This)->lpVtbl->Release(This)
#define IPropertyPageSite_OnStatusChange(This,dwFlags) (This)->lpVtbl->OnStatusChange(This,dwFlags)
#define IPropertyPageSite_GetLocaleID(This,pLocaleID) (This)->lpVtbl->GetLocaleID(This,pLocaleID)
#define IPropertyPageSite_GetPageContainer(This,ppUnk) (This)->lpVtbl->GetPageContainer(This,ppUnk)
#define IPropertyPageSite_TranslateAccelerator(This,pMsg) (This)->lpVtbl->TranslateAccelerator(This,pMsg)
#endif
#endif
  HRESULT WINAPI IPropertyPageSite_OnStatusChange_Proxy(IPropertyPageSite *This,DWORD dwFlags);
  void __RPC_STUB IPropertyPageSite_OnStatusChange_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPageSite_GetLocaleID_Proxy(IPropertyPageSite *This,LCID *pLocaleID);
  void __RPC_STUB IPropertyPageSite_GetLocaleID_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPageSite_GetPageContainer_Proxy(IPropertyPageSite *This,IUnknown **ppUnk);
  void __RPC_STUB IPropertyPageSite_GetPageContainer_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyPageSite_TranslateAccelerator_Proxy(IPropertyPageSite *This,MSG *pMsg);
  void __RPC_STUB IPropertyPageSite_TranslateAccelerator_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPropertyNotifySink_INTERFACE_DEFINED__
#define __IPropertyNotifySink_INTERFACE_DEFINED__
  typedef IPropertyNotifySink *LPPROPERTYNOTIFYSINK;

  EXTERN_C const IID IID_IPropertyNotifySink;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPropertyNotifySink : public IUnknown {
  public:
    virtual HRESULT WINAPI OnChanged(DISPID dispID) = 0;
    virtual HRESULT WINAPI OnRequestEdit(DISPID dispID) = 0;
  };
#else
  typedef struct IPropertyNotifySinkVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPropertyNotifySink *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPropertyNotifySink *This);
      ULONG (WINAPI *Release)(IPropertyNotifySink *This);
      HRESULT (WINAPI *OnChanged)(IPropertyNotifySink *This,DISPID dispID);
      HRESULT (WINAPI *OnRequestEdit)(IPropertyNotifySink *This,DISPID dispID);
    END_INTERFACE
  } IPropertyNotifySinkVtbl;
  struct IPropertyNotifySink {
    CONST_VTBL struct IPropertyNotifySinkVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPropertyNotifySink_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPropertyNotifySink_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPropertyNotifySink_Release(This) (This)->lpVtbl->Release(This)
#define IPropertyNotifySink_OnChanged(This,dispID) (This)->lpVtbl->OnChanged(This,dispID)
#define IPropertyNotifySink_OnRequestEdit(This,dispID) (This)->lpVtbl->OnRequestEdit(This,dispID)
#endif
#endif
  HRESULT WINAPI IPropertyNotifySink_OnChanged_Proxy(IPropertyNotifySink *This,DISPID dispID);
  void __RPC_STUB IPropertyNotifySink_OnChanged_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyNotifySink_OnRequestEdit_Proxy(IPropertyNotifySink *This,DISPID dispID);
  void __RPC_STUB IPropertyNotifySink_OnRequestEdit_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __ISpecifyPropertyPages_INTERFACE_DEFINED__
#define __ISpecifyPropertyPages_INTERFACE_DEFINED__
  typedef ISpecifyPropertyPages *LPSPECIFYPROPERTYPAGES;

  typedef struct tagCAUUID {
    ULONG cElems;
    GUID *pElems;
  } CAUUID;

  typedef struct tagCAUUID *LPCAUUID;

  EXTERN_C const IID IID_ISpecifyPropertyPages;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct ISpecifyPropertyPages : public IUnknown {
  public:
    virtual HRESULT WINAPI GetPages(CAUUID *pPages) = 0;
  };
#else
  typedef struct ISpecifyPropertyPagesVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(ISpecifyPropertyPages *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(ISpecifyPropertyPages *This);
      ULONG (WINAPI *Release)(ISpecifyPropertyPages *This);
      HRESULT (WINAPI *GetPages)(ISpecifyPropertyPages *This,CAUUID *pPages);
    END_INTERFACE
  } ISpecifyPropertyPagesVtbl;
  struct ISpecifyPropertyPages {
    CONST_VTBL struct ISpecifyPropertyPagesVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define ISpecifyPropertyPages_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ISpecifyPropertyPages_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ISpecifyPropertyPages_Release(This) (This)->lpVtbl->Release(This)
#define ISpecifyPropertyPages_GetPages(This,pPages) (This)->lpVtbl->GetPages(This,pPages)
#endif
#endif
  HRESULT WINAPI ISpecifyPropertyPages_GetPages_Proxy(ISpecifyPropertyPages *This,CAUUID *pPages);
  void __RPC_STUB ISpecifyPropertyPages_GetPages_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPersistMemory_INTERFACE_DEFINED__
#define __IPersistMemory_INTERFACE_DEFINED__
  typedef IPersistMemory *LPPERSISTMEMORY;

  EXTERN_C const IID IID_IPersistMemory;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPersistMemory : public IPersist {
  public:
    virtual HRESULT WINAPI IsDirty(void) = 0;
    virtual HRESULT WINAPI Load(LPVOID pMem,ULONG cbSize) = 0;
    virtual HRESULT WINAPI Save(LPVOID pMem,WINBOOL fClearDirty,ULONG cbSize) = 0;
    virtual HRESULT WINAPI GetSizeMax(ULONG *pCbSize) = 0;
    virtual HRESULT WINAPI InitNew(void) = 0;
  };
#else
  typedef struct IPersistMemoryVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPersistMemory *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPersistMemory *This);
      ULONG (WINAPI *Release)(IPersistMemory *This);
      HRESULT (WINAPI *GetClassID)(IPersistMemory *This,CLSID *pClassID);
      HRESULT (WINAPI *IsDirty)(IPersistMemory *This);
      HRESULT (WINAPI *Load)(IPersistMemory *This,LPVOID pMem,ULONG cbSize);
      HRESULT (WINAPI *Save)(IPersistMemory *This,LPVOID pMem,WINBOOL fClearDirty,ULONG cbSize);
      HRESULT (WINAPI *GetSizeMax)(IPersistMemory *This,ULONG *pCbSize);
      HRESULT (WINAPI *InitNew)(IPersistMemory *This);
    END_INTERFACE
  } IPersistMemoryVtbl;
  struct IPersistMemory {
    CONST_VTBL struct IPersistMemoryVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPersistMemory_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPersistMemory_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPersistMemory_Release(This) (This)->lpVtbl->Release(This)
#define IPersistMemory_GetClassID(This,pClassID) (This)->lpVtbl->GetClassID(This,pClassID)
#define IPersistMemory_IsDirty(This) (This)->lpVtbl->IsDirty(This)
#define IPersistMemory_Load(This,pMem,cbSize) (This)->lpVtbl->Load(This,pMem,cbSize)
#define IPersistMemory_Save(This,pMem,fClearDirty,cbSize) (This)->lpVtbl->Save(This,pMem,fClearDirty,cbSize)
#define IPersistMemory_GetSizeMax(This,pCbSize) (This)->lpVtbl->GetSizeMax(This,pCbSize)
#define IPersistMemory_InitNew(This) (This)->lpVtbl->InitNew(This)
#endif
#endif
  HRESULT WINAPI IPersistMemory_IsDirty_Proxy(IPersistMemory *This);
  void __RPC_STUB IPersistMemory_IsDirty_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistMemory_RemoteLoad_Proxy(IPersistMemory *This,BYTE *pMem,ULONG cbSize);
  void __RPC_STUB IPersistMemory_RemoteLoad_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistMemory_RemoteSave_Proxy(IPersistMemory *This,BYTE *pMem,WINBOOL fClearDirty,ULONG cbSize);
  void __RPC_STUB IPersistMemory_RemoteSave_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistMemory_GetSizeMax_Proxy(IPersistMemory *This,ULONG *pCbSize);
  void __RPC_STUB IPersistMemory_GetSizeMax_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistMemory_InitNew_Proxy(IPersistMemory *This);
  void __RPC_STUB IPersistMemory_InitNew_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPersistStreamInit_INTERFACE_DEFINED__
#define __IPersistStreamInit_INTERFACE_DEFINED__
  typedef IPersistStreamInit *LPPERSISTSTREAMINIT;

  EXTERN_C const IID IID_IPersistStreamInit;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPersistStreamInit : public IPersist {
  public:
    virtual HRESULT WINAPI IsDirty(void) = 0;
    virtual HRESULT WINAPI Load(LPSTREAM pStm) = 0;
    virtual HRESULT WINAPI Save(LPSTREAM pStm,WINBOOL fClearDirty) = 0;
    virtual HRESULT WINAPI GetSizeMax(ULARGE_INTEGER *pCbSize) = 0;
    virtual HRESULT WINAPI InitNew(void) = 0;
  };
#else
  typedef struct IPersistStreamInitVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPersistStreamInit *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPersistStreamInit *This);
      ULONG (WINAPI *Release)(IPersistStreamInit *This);
      HRESULT (WINAPI *GetClassID)(IPersistStreamInit *This,CLSID *pClassID);
      HRESULT (WINAPI *IsDirty)(IPersistStreamInit *This);
      HRESULT (WINAPI *Load)(IPersistStreamInit *This,LPSTREAM pStm);
      HRESULT (WINAPI *Save)(IPersistStreamInit *This,LPSTREAM pStm,WINBOOL fClearDirty);
      HRESULT (WINAPI *GetSizeMax)(IPersistStreamInit *This,ULARGE_INTEGER *pCbSize);
      HRESULT (WINAPI *InitNew)(IPersistStreamInit *This);
    END_INTERFACE
  } IPersistStreamInitVtbl;
  struct IPersistStreamInit {
    CONST_VTBL struct IPersistStreamInitVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPersistStreamInit_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPersistStreamInit_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPersistStreamInit_Release(This) (This)->lpVtbl->Release(This)
#define IPersistStreamInit_GetClassID(This,pClassID) (This)->lpVtbl->GetClassID(This,pClassID)
#define IPersistStreamInit_IsDirty(This) (This)->lpVtbl->IsDirty(This)
#define IPersistStreamInit_Load(This,pStm) (This)->lpVtbl->Load(This,pStm)
#define IPersistStreamInit_Save(This,pStm,fClearDirty) (This)->lpVtbl->Save(This,pStm,fClearDirty)
#define IPersistStreamInit_GetSizeMax(This,pCbSize) (This)->lpVtbl->GetSizeMax(This,pCbSize)
#define IPersistStreamInit_InitNew(This) (This)->lpVtbl->InitNew(This)
#endif
#endif
  HRESULT WINAPI IPersistStreamInit_IsDirty_Proxy(IPersistStreamInit *This);
  void __RPC_STUB IPersistStreamInit_IsDirty_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistStreamInit_Load_Proxy(IPersistStreamInit *This,LPSTREAM pStm);
  void __RPC_STUB IPersistStreamInit_Load_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistStreamInit_Save_Proxy(IPersistStreamInit *This,LPSTREAM pStm,WINBOOL fClearDirty);
  void __RPC_STUB IPersistStreamInit_Save_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistStreamInit_GetSizeMax_Proxy(IPersistStreamInit *This,ULARGE_INTEGER *pCbSize);
  void __RPC_STUB IPersistStreamInit_GetSizeMax_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistStreamInit_InitNew_Proxy(IPersistStreamInit *This);
  void __RPC_STUB IPersistStreamInit_InitNew_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPersistPropertyBag_INTERFACE_DEFINED__
#define __IPersistPropertyBag_INTERFACE_DEFINED__
  typedef IPersistPropertyBag *LPPERSISTPROPERTYBAG;

  EXTERN_C const IID IID_IPersistPropertyBag;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPersistPropertyBag : public IPersist {
  public:
    virtual HRESULT WINAPI InitNew(void) = 0;
    virtual HRESULT WINAPI Load(IPropertyBag *pPropBag,IErrorLog *pErrorLog) = 0;
    virtual HRESULT WINAPI Save(IPropertyBag *pPropBag,WINBOOL fClearDirty,WINBOOL fSaveAllProperties) = 0;
  };
#else
  typedef struct IPersistPropertyBagVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPersistPropertyBag *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPersistPropertyBag *This);
      ULONG (WINAPI *Release)(IPersistPropertyBag *This);
      HRESULT (WINAPI *GetClassID)(IPersistPropertyBag *This,CLSID *pClassID);
      HRESULT (WINAPI *InitNew)(IPersistPropertyBag *This);
      HRESULT (WINAPI *Load)(IPersistPropertyBag *This,IPropertyBag *pPropBag,IErrorLog *pErrorLog);
      HRESULT (WINAPI *Save)(IPersistPropertyBag *This,IPropertyBag *pPropBag,WINBOOL fClearDirty,WINBOOL fSaveAllProperties);
    END_INTERFACE
  } IPersistPropertyBagVtbl;
  struct IPersistPropertyBag {
    CONST_VTBL struct IPersistPropertyBagVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPersistPropertyBag_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPersistPropertyBag_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPersistPropertyBag_Release(This) (This)->lpVtbl->Release(This)
#define IPersistPropertyBag_GetClassID(This,pClassID) (This)->lpVtbl->GetClassID(This,pClassID)
#define IPersistPropertyBag_InitNew(This) (This)->lpVtbl->InitNew(This)
#define IPersistPropertyBag_Load(This,pPropBag,pErrorLog) (This)->lpVtbl->Load(This,pPropBag,pErrorLog)
#define IPersistPropertyBag_Save(This,pPropBag,fClearDirty,fSaveAllProperties) (This)->lpVtbl->Save(This,pPropBag,fClearDirty,fSaveAllProperties)
#endif
#endif
  HRESULT WINAPI IPersistPropertyBag_InitNew_Proxy(IPersistPropertyBag *This);
  void __RPC_STUB IPersistPropertyBag_InitNew_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistPropertyBag_Load_Proxy(IPersistPropertyBag *This,IPropertyBag *pPropBag,IErrorLog *pErrorLog);
  void __RPC_STUB IPersistPropertyBag_Load_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistPropertyBag_Save_Proxy(IPersistPropertyBag *This,IPropertyBag *pPropBag,WINBOOL fClearDirty,WINBOOL fSaveAllProperties);
  void __RPC_STUB IPersistPropertyBag_Save_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __ISimpleFrameSite_INTERFACE_DEFINED__
#define __ISimpleFrameSite_INTERFACE_DEFINED__
  typedef ISimpleFrameSite *LPSIMPLEFRAMESITE;

  EXTERN_C const IID IID_ISimpleFrameSite;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct ISimpleFrameSite : public IUnknown {
  public:
    virtual HRESULT WINAPI PreMessageFilter(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp,LRESULT *plResult,DWORD *pdwCookie) = 0;
    virtual HRESULT WINAPI PostMessageFilter(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp,LRESULT *plResult,DWORD dwCookie) = 0;
  };
#else
  typedef struct ISimpleFrameSiteVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(ISimpleFrameSite *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(ISimpleFrameSite *This);
      ULONG (WINAPI *Release)(ISimpleFrameSite *This);
      HRESULT (WINAPI *PreMessageFilter)(ISimpleFrameSite *This,HWND hWnd,UINT msg,WPARAM wp,LPARAM lp,LRESULT *plResult,DWORD *pdwCookie);
      HRESULT (WINAPI *PostMessageFilter)(ISimpleFrameSite *This,HWND hWnd,UINT msg,WPARAM wp,LPARAM lp,LRESULT *plResult,DWORD dwCookie);
    END_INTERFACE
  } ISimpleFrameSiteVtbl;
  struct ISimpleFrameSite {
    CONST_VTBL struct ISimpleFrameSiteVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define ISimpleFrameSite_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ISimpleFrameSite_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ISimpleFrameSite_Release(This) (This)->lpVtbl->Release(This)
#define ISimpleFrameSite_PreMessageFilter(This,hWnd,msg,wp,lp,plResult,pdwCookie) (This)->lpVtbl->PreMessageFilter(This,hWnd,msg,wp,lp,plResult,pdwCookie)
#define ISimpleFrameSite_PostMessageFilter(This,hWnd,msg,wp,lp,plResult,dwCookie) (This)->lpVtbl->PostMessageFilter(This,hWnd,msg,wp,lp,plResult,dwCookie)
#endif
#endif
  HRESULT WINAPI ISimpleFrameSite_PreMessageFilter_Proxy(ISimpleFrameSite *This,HWND hWnd,UINT msg,WPARAM wp,LPARAM lp,LRESULT *plResult,DWORD *pdwCookie);
  void __RPC_STUB ISimpleFrameSite_PreMessageFilter_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ISimpleFrameSite_PostMessageFilter_Proxy(ISimpleFrameSite *This,HWND hWnd,UINT msg,WPARAM wp,LPARAM lp,LRESULT *plResult,DWORD dwCookie);
  void __RPC_STUB ISimpleFrameSite_PostMessageFilter_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IFont_INTERFACE_DEFINED__
#define __IFont_INTERFACE_DEFINED__
  typedef IFont *LPFONT;

#ifndef OLE2ANSI
  typedef TEXTMETRICW TEXTMETRICOLE;
#else
  typedef TEXTMETRIC TEXTMETRICOLE;
#endif
  typedef TEXTMETRICOLE *LPTEXTMETRICOLE;

  EXTERN_C const IID IID_IFont;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IFont : public IUnknown {
  public:
    virtual HRESULT WINAPI get_Name(BSTR *pName) = 0;
    virtual HRESULT WINAPI put_Name(BSTR name) = 0;
    virtual HRESULT WINAPI get_Size(CY *pSize) = 0;
    virtual HRESULT WINAPI put_Size(CY size) = 0;
    virtual HRESULT WINAPI get_Bold(WINBOOL *pBold) = 0;
    virtual HRESULT WINAPI put_Bold(WINBOOL bold) = 0;
    virtual HRESULT WINAPI get_Italic(WINBOOL *pItalic) = 0;
    virtual HRESULT WINAPI put_Italic(WINBOOL italic) = 0;
    virtual HRESULT WINAPI get_Underline(WINBOOL *pUnderline) = 0;
    virtual HRESULT WINAPI put_Underline(WINBOOL underline) = 0;
    virtual HRESULT WINAPI get_Strikethrough(WINBOOL *pStrikethrough) = 0;
    virtual HRESULT WINAPI put_Strikethrough(WINBOOL strikethrough) = 0;
    virtual HRESULT WINAPI get_Weight(SHORT *pWeight) = 0;
    virtual HRESULT WINAPI put_Weight(SHORT weight) = 0;
    virtual HRESULT WINAPI get_Charset(SHORT *pCharset) = 0;
    virtual HRESULT WINAPI put_Charset(SHORT charset) = 0;
    virtual HRESULT WINAPI get_hFont(HFONT *phFont) = 0;
    virtual HRESULT WINAPI Clone(IFont **ppFont) = 0;
    virtual HRESULT WINAPI IsEqual(IFont *pFontOther) = 0;
    virtual HRESULT WINAPI SetRatio(LONG cyLogical,LONG cyHimetric) = 0;
    virtual HRESULT WINAPI QueryTextMetrics(TEXTMETRICOLE *pTM) = 0;
    virtual HRESULT WINAPI AddRefHfont(HFONT hFont) = 0;
    virtual HRESULT WINAPI ReleaseHfont(HFONT hFont) = 0;
    virtual HRESULT WINAPI SetHdc(HDC hDC) = 0;
  };
#else
  typedef struct IFontVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IFont *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IFont *This);
      ULONG (WINAPI *Release)(IFont *This);
      HRESULT (WINAPI *get_Name)(IFont *This,BSTR *pName);
      HRESULT (WINAPI *put_Name)(IFont *This,BSTR name);
      HRESULT (WINAPI *get_Size)(IFont *This,CY *pSize);
      HRESULT (WINAPI *put_Size)(IFont *This,CY size);
      HRESULT (WINAPI *get_Bold)(IFont *This,WINBOOL *pBold);
      HRESULT (WINAPI *put_Bold)(IFont *This,WINBOOL bold);
      HRESULT (WINAPI *get_Italic)(IFont *This,WINBOOL *pItalic);
      HRESULT (WINAPI *put_Italic)(IFont *This,WINBOOL italic);
      HRESULT (WINAPI *get_Underline)(IFont *This,WINBOOL *pUnderline);
      HRESULT (WINAPI *put_Underline)(IFont *This,WINBOOL underline);
      HRESULT (WINAPI *get_Strikethrough)(IFont *This,WINBOOL *pStrikethrough);
      HRESULT (WINAPI *put_Strikethrough)(IFont *This,WINBOOL strikethrough);
      HRESULT (WINAPI *get_Weight)(IFont *This,SHORT *pWeight);
      HRESULT (WINAPI *put_Weight)(IFont *This,SHORT weight);
      HRESULT (WINAPI *get_Charset)(IFont *This,SHORT *pCharset);
      HRESULT (WINAPI *put_Charset)(IFont *This,SHORT charset);
      HRESULT (WINAPI *get_hFont)(IFont *This,HFONT *phFont);
      HRESULT (WINAPI *Clone)(IFont *This,IFont **ppFont);
      HRESULT (WINAPI *IsEqual)(IFont *This,IFont *pFontOther);
      HRESULT (WINAPI *SetRatio)(IFont *This,LONG cyLogical,LONG cyHimetric);
      HRESULT (WINAPI *QueryTextMetrics)(IFont *This,TEXTMETRICOLE *pTM);
      HRESULT (WINAPI *AddRefHfont)(IFont *This,HFONT hFont);
      HRESULT (WINAPI *ReleaseHfont)(IFont *This,HFONT hFont);
      HRESULT (WINAPI *SetHdc)(IFont *This,HDC hDC);
    END_INTERFACE
  } IFontVtbl;
  struct IFont {
    CONST_VTBL struct IFontVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IFont_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IFont_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IFont_Release(This) (This)->lpVtbl->Release(This)
#define IFont_get_Name(This,pName) (This)->lpVtbl->get_Name(This,pName)
#define IFont_put_Name(This,name) (This)->lpVtbl->put_Name(This,name)
#define IFont_get_Size(This,pSize) (This)->lpVtbl->get_Size(This,pSize)
#define IFont_put_Size(This,size) (This)->lpVtbl->put_Size(This,size)
#define IFont_get_Bold(This,pBold) (This)->lpVtbl->get_Bold(This,pBold)
#define IFont_put_Bold(This,bold) (This)->lpVtbl->put_Bold(This,bold)
#define IFont_get_Italic(This,pItalic) (This)->lpVtbl->get_Italic(This,pItalic)
#define IFont_put_Italic(This,italic) (This)->lpVtbl->put_Italic(This,italic)
#define IFont_get_Underline(This,pUnderline) (This)->lpVtbl->get_Underline(This,pUnderline)
#define IFont_put_Underline(This,underline) (This)->lpVtbl->put_Underline(This,underline)
#define IFont_get_Strikethrough(This,pStrikethrough) (This)->lpVtbl->get_Strikethrough(This,pStrikethrough)
#define IFont_put_Strikethrough(This,strikethrough) (This)->lpVtbl->put_Strikethrough(This,strikethrough)
#define IFont_get_Weight(This,pWeight) (This)->lpVtbl->get_Weight(This,pWeight)
#define IFont_put_Weight(This,weight) (This)->lpVtbl->put_Weight(This,weight)
#define IFont_get_Charset(This,pCharset) (This)->lpVtbl->get_Charset(This,pCharset)
#define IFont_put_Charset(This,charset) (This)->lpVtbl->put_Charset(This,charset)
#define IFont_get_hFont(This,phFont) (This)->lpVtbl->get_hFont(This,phFont)
#define IFont_Clone(This,ppFont) (This)->lpVtbl->Clone(This,ppFont)
#define IFont_IsEqual(This,pFontOther) (This)->lpVtbl->IsEqual(This,pFontOther)
#define IFont_SetRatio(This,cyLogical,cyHimetric) (This)->lpVtbl->SetRatio(This,cyLogical,cyHimetric)
#define IFont_QueryTextMetrics(This,pTM) (This)->lpVtbl->QueryTextMetrics(This,pTM)
#define IFont_AddRefHfont(This,hFont) (This)->lpVtbl->AddRefHfont(This,hFont)
#define IFont_ReleaseHfont(This,hFont) (This)->lpVtbl->ReleaseHfont(This,hFont)
#define IFont_SetHdc(This,hDC) (This)->lpVtbl->SetHdc(This,hDC)
#endif
#endif
  HRESULT WINAPI IFont_get_Name_Proxy(IFont *This,BSTR *pName);
  void __RPC_STUB IFont_get_Name_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_put_Name_Proxy(IFont *This,BSTR name);
  void __RPC_STUB IFont_put_Name_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_get_Size_Proxy(IFont *This,CY *pSize);
  void __RPC_STUB IFont_get_Size_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_put_Size_Proxy(IFont *This,CY size);
  void __RPC_STUB IFont_put_Size_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_get_Bold_Proxy(IFont *This,WINBOOL *pBold);
  void __RPC_STUB IFont_get_Bold_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_put_Bold_Proxy(IFont *This,WINBOOL bold);
  void __RPC_STUB IFont_put_Bold_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_get_Italic_Proxy(IFont *This,WINBOOL *pItalic);
  void __RPC_STUB IFont_get_Italic_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_put_Italic_Proxy(IFont *This,WINBOOL italic);
  void __RPC_STUB IFont_put_Italic_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_get_Underline_Proxy(IFont *This,WINBOOL *pUnderline);
  void __RPC_STUB IFont_get_Underline_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_put_Underline_Proxy(IFont *This,WINBOOL underline);
  void __RPC_STUB IFont_put_Underline_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_get_Strikethrough_Proxy(IFont *This,WINBOOL *pStrikethrough);
  void __RPC_STUB IFont_get_Strikethrough_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_put_Strikethrough_Proxy(IFont *This,WINBOOL strikethrough);
  void __RPC_STUB IFont_put_Strikethrough_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_get_Weight_Proxy(IFont *This,SHORT *pWeight);
  void __RPC_STUB IFont_get_Weight_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_put_Weight_Proxy(IFont *This,SHORT weight);
  void __RPC_STUB IFont_put_Weight_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_get_Charset_Proxy(IFont *This,SHORT *pCharset);
  void __RPC_STUB IFont_get_Charset_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_put_Charset_Proxy(IFont *This,SHORT charset);
  void __RPC_STUB IFont_put_Charset_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_get_hFont_Proxy(IFont *This,HFONT *phFont);
  void __RPC_STUB IFont_get_hFont_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_Clone_Proxy(IFont *This,IFont **ppFont);
  void __RPC_STUB IFont_Clone_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_IsEqual_Proxy(IFont *This,IFont *pFontOther);
  void __RPC_STUB IFont_IsEqual_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_SetRatio_Proxy(IFont *This,LONG cyLogical,LONG cyHimetric);
  void __RPC_STUB IFont_SetRatio_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_QueryTextMetrics_Proxy(IFont *This,TEXTMETRICOLE *pTM);
  void __RPC_STUB IFont_QueryTextMetrics_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_AddRefHfont_Proxy(IFont *This,HFONT hFont);
  void __RPC_STUB IFont_AddRefHfont_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_ReleaseHfont_Proxy(IFont *This,HFONT hFont);
  void __RPC_STUB IFont_ReleaseHfont_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IFont_SetHdc_Proxy(IFont *This,HDC hDC);
  void __RPC_STUB IFont_SetHdc_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPicture_INTERFACE_DEFINED__
#define __IPicture_INTERFACE_DEFINED__
  typedef IPicture *LPPICTURE;

  typedef enum tagPictureAttributes {
    PICTURE_SCALABLE = 0x1,PICTURE_TRANSPARENT = 0x2
  } PICTUREATTRIBUTES;

  typedef UINT OLE_HANDLE;
  typedef LONG OLE_XPOS_HIMETRIC;
  typedef LONG OLE_YPOS_HIMETRIC;
  typedef LONG OLE_XSIZE_HIMETRIC;
  typedef LONG OLE_YSIZE_HIMETRIC;

  EXTERN_C const IID IID_IPicture;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPicture : public IUnknown {
  public:
    virtual HRESULT WINAPI get_Handle(OLE_HANDLE *pHandle) = 0;
    virtual HRESULT WINAPI get_hPal(OLE_HANDLE *phPal) = 0;
    virtual HRESULT WINAPI get_Type(SHORT *pType) = 0;
    virtual HRESULT WINAPI get_Width(OLE_XSIZE_HIMETRIC *pWidth) = 0;
    virtual HRESULT WINAPI get_Height(OLE_YSIZE_HIMETRIC *pHeight) = 0;
    virtual HRESULT WINAPI Render(HDC hDC,LONG x,LONG y,LONG cx,LONG cy,OLE_XPOS_HIMETRIC xSrc,OLE_YPOS_HIMETRIC ySrc,OLE_XSIZE_HIMETRIC cxSrc,OLE_YSIZE_HIMETRIC cySrc,LPCRECT pRcWBounds) = 0;
    virtual HRESULT WINAPI set_hPal(OLE_HANDLE hPal) = 0;
    virtual HRESULT WINAPI get_CurDC(HDC *phDC) = 0;
    virtual HRESULT WINAPI SelectPicture(HDC hDCIn,HDC *phDCOut,OLE_HANDLE *phBmpOut) = 0;
    virtual HRESULT WINAPI get_KeepOriginalFormat(WINBOOL *pKeep) = 0;
    virtual HRESULT WINAPI put_KeepOriginalFormat(WINBOOL keep) = 0;
    virtual HRESULT WINAPI PictureChanged(void) = 0;
    virtual HRESULT WINAPI SaveAsFile(LPSTREAM pStream,WINBOOL fSaveMemCopy,LONG *pCbSize) = 0;
    virtual HRESULT WINAPI get_Attributes(DWORD *pDwAttr) = 0;
  };
#else
  typedef struct IPictureVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPicture *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPicture *This);
      ULONG (WINAPI *Release)(IPicture *This);
      HRESULT (WINAPI *get_Handle)(IPicture *This,OLE_HANDLE *pHandle);
      HRESULT (WINAPI *get_hPal)(IPicture *This,OLE_HANDLE *phPal);
      HRESULT (WINAPI *get_Type)(IPicture *This,SHORT *pType);
      HRESULT (WINAPI *get_Width)(IPicture *This,OLE_XSIZE_HIMETRIC *pWidth);
      HRESULT (WINAPI *get_Height)(IPicture *This,OLE_YSIZE_HIMETRIC *pHeight);
      HRESULT (WINAPI *Render)(IPicture *This,HDC hDC,LONG x,LONG y,LONG cx,LONG cy,OLE_XPOS_HIMETRIC xSrc,OLE_YPOS_HIMETRIC ySrc,OLE_XSIZE_HIMETRIC cxSrc,OLE_YSIZE_HIMETRIC cySrc,LPCRECT pRcWBounds);
      HRESULT (WINAPI *set_hPal)(IPicture *This,OLE_HANDLE hPal);
      HRESULT (WINAPI *get_CurDC)(IPicture *This,HDC *phDC);
      HRESULT (WINAPI *SelectPicture)(IPicture *This,HDC hDCIn,HDC *phDCOut,OLE_HANDLE *phBmpOut);
      HRESULT (WINAPI *get_KeepOriginalFormat)(IPicture *This,WINBOOL *pKeep);
      HRESULT (WINAPI *put_KeepOriginalFormat)(IPicture *This,WINBOOL keep);
      HRESULT (WINAPI *PictureChanged)(IPicture *This);
      HRESULT (WINAPI *SaveAsFile)(IPicture *This,LPSTREAM pStream,WINBOOL fSaveMemCopy,LONG *pCbSize);
      HRESULT (WINAPI *get_Attributes)(IPicture *This,DWORD *pDwAttr);
    END_INTERFACE
  } IPictureVtbl;
  struct IPicture {
    CONST_VTBL struct IPictureVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPicture_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPicture_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPicture_Release(This) (This)->lpVtbl->Release(This)
#define IPicture_get_Handle(This,pHandle) (This)->lpVtbl->get_Handle(This,pHandle)
#define IPicture_get_hPal(This,phPal) (This)->lpVtbl->get_hPal(This,phPal)
#define IPicture_get_Type(This,pType) (This)->lpVtbl->get_Type(This,pType)
#define IPicture_get_Width(This,pWidth) (This)->lpVtbl->get_Width(This,pWidth)
#define IPicture_get_Height(This,pHeight) (This)->lpVtbl->get_Height(This,pHeight)
#define IPicture_Render(This,hDC,x,y,cx,cy,xSrc,ySrc,cxSrc,cySrc,pRcWBounds) (This)->lpVtbl->Render(This,hDC,x,y,cx,cy,xSrc,ySrc,cxSrc,cySrc,pRcWBounds)
#define IPicture_set_hPal(This,hPal) (This)->lpVtbl->set_hPal(This,hPal)
#define IPicture_get_CurDC(This,phDC) (This)->lpVtbl->get_CurDC(This,phDC)
#define IPicture_SelectPicture(This,hDCIn,phDCOut,phBmpOut) (This)->lpVtbl->SelectPicture(This,hDCIn,phDCOut,phBmpOut)
#define IPicture_get_KeepOriginalFormat(This,pKeep) (This)->lpVtbl->get_KeepOriginalFormat(This,pKeep)
#define IPicture_put_KeepOriginalFormat(This,keep) (This)->lpVtbl->put_KeepOriginalFormat(This,keep)
#define IPicture_PictureChanged(This) (This)->lpVtbl->PictureChanged(This)
#define IPicture_SaveAsFile(This,pStream,fSaveMemCopy,pCbSize) (This)->lpVtbl->SaveAsFile(This,pStream,fSaveMemCopy,pCbSize)
#define IPicture_get_Attributes(This,pDwAttr) (This)->lpVtbl->get_Attributes(This,pDwAttr)
#endif
#endif
  HRESULT WINAPI IPicture_get_Handle_Proxy(IPicture *This,OLE_HANDLE *pHandle);
  void __RPC_STUB IPicture_get_Handle_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_get_hPal_Proxy(IPicture *This,OLE_HANDLE *phPal);
  void __RPC_STUB IPicture_get_hPal_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_get_Type_Proxy(IPicture *This,SHORT *pType);
  void __RPC_STUB IPicture_get_Type_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_get_Width_Proxy(IPicture *This,OLE_XSIZE_HIMETRIC *pWidth);
  void __RPC_STUB IPicture_get_Width_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_get_Height_Proxy(IPicture *This,OLE_YSIZE_HIMETRIC *pHeight);
  void __RPC_STUB IPicture_get_Height_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_Render_Proxy(IPicture *This,HDC hDC,LONG x,LONG y,LONG cx,LONG cy,OLE_XPOS_HIMETRIC xSrc,OLE_YPOS_HIMETRIC ySrc,OLE_XSIZE_HIMETRIC cxSrc,OLE_YSIZE_HIMETRIC cySrc,LPCRECT pRcWBounds);
  void __RPC_STUB IPicture_Render_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_set_hPal_Proxy(IPicture *This,OLE_HANDLE hPal);
  void __RPC_STUB IPicture_set_hPal_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_get_CurDC_Proxy(IPicture *This,HDC *phDC);
  void __RPC_STUB IPicture_get_CurDC_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_SelectPicture_Proxy(IPicture *This,HDC hDCIn,HDC *phDCOut,OLE_HANDLE *phBmpOut);
  void __RPC_STUB IPicture_SelectPicture_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_get_KeepOriginalFormat_Proxy(IPicture *This,WINBOOL *pKeep);
  void __RPC_STUB IPicture_get_KeepOriginalFormat_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_put_KeepOriginalFormat_Proxy(IPicture *This,WINBOOL keep);
  void __RPC_STUB IPicture_put_KeepOriginalFormat_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_PictureChanged_Proxy(IPicture *This);
  void __RPC_STUB IPicture_PictureChanged_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_SaveAsFile_Proxy(IPicture *This,LPSTREAM pStream,WINBOOL fSaveMemCopy,LONG *pCbSize);
  void __RPC_STUB IPicture_SaveAsFile_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPicture_get_Attributes_Proxy(IPicture *This,DWORD *pDwAttr);
  void __RPC_STUB IPicture_get_Attributes_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IFontEventsDisp_INTERFACE_DEFINED__
#define __IFontEventsDisp_INTERFACE_DEFINED__
  typedef IFontEventsDisp *LPFONTEVENTS;

  EXTERN_C const IID IID_IFontEventsDisp;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IFontEventsDisp : public IDispatch {
  };
#else
  typedef struct IFontEventsDispVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IFontEventsDisp *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IFontEventsDisp *This);
      ULONG (WINAPI *Release)(IFontEventsDisp *This);
      HRESULT (WINAPI *GetTypeInfoCount)(IFontEventsDisp *This,UINT *pctinfo);
      HRESULT (WINAPI *GetTypeInfo)(IFontEventsDisp *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
      HRESULT (WINAPI *GetIDsOfNames)(IFontEventsDisp *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
      HRESULT (WINAPI *Invoke)(IFontEventsDisp *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
    END_INTERFACE
  } IFontEventsDispVtbl;
  struct IFontEventsDisp {
    CONST_VTBL struct IFontEventsDispVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IFontEventsDisp_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IFontEventsDisp_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IFontEventsDisp_Release(This) (This)->lpVtbl->Release(This)
#define IFontEventsDisp_GetTypeInfoCount(This,pctinfo) (This)->lpVtbl->GetTypeInfoCount(This,pctinfo)
#define IFontEventsDisp_GetTypeInfo(This,iTInfo,lcid,ppTInfo) (This)->lpVtbl->GetTypeInfo(This,iTInfo,lcid,ppTInfo)
#define IFontEventsDisp_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) (This)->lpVtbl->GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)
#define IFontEventsDisp_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) (This)->lpVtbl->Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)
#endif
#endif
#endif

#ifndef __IFontDisp_INTERFACE_DEFINED__
#define __IFontDisp_INTERFACE_DEFINED__
  typedef IFontDisp *LPFONTDISP;

  EXTERN_C const IID IID_IFontDisp;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IFontDisp : public IDispatch {
  };
#else
  typedef struct IFontDispVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IFontDisp *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IFontDisp *This);
      ULONG (WINAPI *Release)(IFontDisp *This);
      HRESULT (WINAPI *GetTypeInfoCount)(IFontDisp *This,UINT *pctinfo);
      HRESULT (WINAPI *GetTypeInfo)(IFontDisp *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
      HRESULT (WINAPI *GetIDsOfNames)(IFontDisp *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
      HRESULT (WINAPI *Invoke)(IFontDisp *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
    END_INTERFACE
  } IFontDispVtbl;
  struct IFontDisp {
    CONST_VTBL struct IFontDispVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IFontDisp_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IFontDisp_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IFontDisp_Release(This) (This)->lpVtbl->Release(This)
#define IFontDisp_GetTypeInfoCount(This,pctinfo) (This)->lpVtbl->GetTypeInfoCount(This,pctinfo)
#define IFontDisp_GetTypeInfo(This,iTInfo,lcid,ppTInfo) (This)->lpVtbl->GetTypeInfo(This,iTInfo,lcid,ppTInfo)
#define IFontDisp_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) (This)->lpVtbl->GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)
#define IFontDisp_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) (This)->lpVtbl->Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)
#endif
#endif
#endif

#ifndef __IPictureDisp_INTERFACE_DEFINED__
#define __IPictureDisp_INTERFACE_DEFINED__
  typedef IPictureDisp *LPPICTUREDISP;

  EXTERN_C const IID IID_IPictureDisp;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPictureDisp : public IDispatch {
  };
#else
  typedef struct IPictureDispVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPictureDisp *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPictureDisp *This);
      ULONG (WINAPI *Release)(IPictureDisp *This);
      HRESULT (WINAPI *GetTypeInfoCount)(IPictureDisp *This,UINT *pctinfo);
      HRESULT (WINAPI *GetTypeInfo)(IPictureDisp *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
      HRESULT (WINAPI *GetIDsOfNames)(IPictureDisp *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
      HRESULT (WINAPI *Invoke)(IPictureDisp *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
    END_INTERFACE
  } IPictureDispVtbl;
  struct IPictureDisp {
    CONST_VTBL struct IPictureDispVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPictureDisp_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPictureDisp_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPictureDisp_Release(This) (This)->lpVtbl->Release(This)
#define IPictureDisp_GetTypeInfoCount(This,pctinfo) (This)->lpVtbl->GetTypeInfoCount(This,pctinfo)
#define IPictureDisp_GetTypeInfo(This,iTInfo,lcid,ppTInfo) (This)->lpVtbl->GetTypeInfo(This,iTInfo,lcid,ppTInfo)
#define IPictureDisp_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) (This)->lpVtbl->GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)
#define IPictureDisp_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) (This)->lpVtbl->Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)
#endif
#endif
#endif

#ifndef __IOleInPlaceObjectWindowless_INTERFACE_DEFINED__
#define __IOleInPlaceObjectWindowless_INTERFACE_DEFINED__
  typedef IOleInPlaceObjectWindowless *LPOLEINPLACEOBJECTWINDOWLESS;

  EXTERN_C const IID IID_IOleInPlaceObjectWindowless;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IOleInPlaceObjectWindowless : public IOleInPlaceObject {
  public:
    virtual HRESULT WINAPI OnWindowMessage(UINT msg,WPARAM wParam,LPARAM lParam,LRESULT *plResult) = 0;
    virtual HRESULT WINAPI GetDropTarget(IDropTarget **ppDropTarget) = 0;
  };
#else
  typedef struct IOleInPlaceObjectWindowlessVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IOleInPlaceObjectWindowless *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IOleInPlaceObjectWindowless *This);
      ULONG (WINAPI *Release)(IOleInPlaceObjectWindowless *This);
      HRESULT (WINAPI *GetWindow)(IOleInPlaceObjectWindowless *This,HWND *phwnd);
      HRESULT (WINAPI *ContextSensitiveHelp)(IOleInPlaceObjectWindowless *This,WINBOOL fEnterMode);
      HRESULT (WINAPI *InPlaceDeactivate)(IOleInPlaceObjectWindowless *This);
      HRESULT (WINAPI *UIDeactivate)(IOleInPlaceObjectWindowless *This);
      HRESULT (WINAPI *SetObjectRects)(IOleInPlaceObjectWindowless *This,LPCRECT lprcPosRect,LPCRECT lprcClipRect);
      HRESULT (WINAPI *ReactivateAndUndo)(IOleInPlaceObjectWindowless *This);
      HRESULT (WINAPI *OnWindowMessage)(IOleInPlaceObjectWindowless *This,UINT msg,WPARAM wParam,LPARAM lParam,LRESULT *plResult);
      HRESULT (WINAPI *GetDropTarget)(IOleInPlaceObjectWindowless *This,IDropTarget **ppDropTarget);
    END_INTERFACE
  } IOleInPlaceObjectWindowlessVtbl;
  struct IOleInPlaceObjectWindowless {
    CONST_VTBL struct IOleInPlaceObjectWindowlessVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IOleInPlaceObjectWindowless_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IOleInPlaceObjectWindowless_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IOleInPlaceObjectWindowless_Release(This) (This)->lpVtbl->Release(This)
#define IOleInPlaceObjectWindowless_GetWindow(This,phwnd) (This)->lpVtbl->GetWindow(This,phwnd)
#define IOleInPlaceObjectWindowless_ContextSensitiveHelp(This,fEnterMode) (This)->lpVtbl->ContextSensitiveHelp(This,fEnterMode)
#define IOleInPlaceObjectWindowless_InPlaceDeactivate(This) (This)->lpVtbl->InPlaceDeactivate(This)
#define IOleInPlaceObjectWindowless_UIDeactivate(This) (This)->lpVtbl->UIDeactivate(This)
#define IOleInPlaceObjectWindowless_SetObjectRects(This,lprcPosRect,lprcClipRect) (This)->lpVtbl->SetObjectRects(This,lprcPosRect,lprcClipRect)
#define IOleInPlaceObjectWindowless_ReactivateAndUndo(This) (This)->lpVtbl->ReactivateAndUndo(This)
#define IOleInPlaceObjectWindowless_OnWindowMessage(This,msg,wParam,lParam,plResult) (This)->lpVtbl->OnWindowMessage(This,msg,wParam,lParam,plResult)
#define IOleInPlaceObjectWindowless_GetDropTarget(This,ppDropTarget) (This)->lpVtbl->GetDropTarget(This,ppDropTarget)
#endif
#endif
  HRESULT WINAPI IOleInPlaceObjectWindowless_OnWindowMessage_Proxy(IOleInPlaceObjectWindowless *This,UINT msg,WPARAM wParam,LPARAM lParam,LRESULT *plResult);
  void __RPC_STUB IOleInPlaceObjectWindowless_OnWindowMessage_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceObjectWindowless_GetDropTarget_Proxy(IOleInPlaceObjectWindowless *This,IDropTarget **ppDropTarget);
  void __RPC_STUB IOleInPlaceObjectWindowless_GetDropTarget_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IOleInPlaceSiteEx_INTERFACE_DEFINED__
#define __IOleInPlaceSiteEx_INTERFACE_DEFINED__
  typedef IOleInPlaceSiteEx *LPOLEINPLACESITEEX;

  typedef enum tagACTIVATEFLAGS {
    ACTIVATE_WINDOWLESS = 1
  } ACTIVATEFLAGS;

  EXTERN_C const IID IID_IOleInPlaceSiteEx;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IOleInPlaceSiteEx : public IOleInPlaceSite {
  public:
    virtual HRESULT WINAPI OnInPlaceActivateEx(WINBOOL *pfNoRedraw,DWORD dwFlags) = 0;
    virtual HRESULT WINAPI OnInPlaceDeactivateEx(WINBOOL fNoRedraw) = 0;
    virtual HRESULT WINAPI RequestUIActivate(void) = 0;
  };
#else
  typedef struct IOleInPlaceSiteExVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IOleInPlaceSiteEx *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IOleInPlaceSiteEx *This);
      ULONG (WINAPI *Release)(IOleInPlaceSiteEx *This);
      HRESULT (WINAPI *GetWindow)(IOleInPlaceSiteEx *This,HWND *phwnd);
      HRESULT (WINAPI *ContextSensitiveHelp)(IOleInPlaceSiteEx *This,WINBOOL fEnterMode);
      HRESULT (WINAPI *CanInPlaceActivate)(IOleInPlaceSiteEx *This);
      HRESULT (WINAPI *OnInPlaceActivate)(IOleInPlaceSiteEx *This);
      HRESULT (WINAPI *OnUIActivate)(IOleInPlaceSiteEx *This);
      HRESULT (WINAPI *GetWindowContext)(IOleInPlaceSiteEx *This,IOleInPlaceFrame **ppFrame,IOleInPlaceUIWindow **ppDoc,LPRECT lprcPosRect,LPRECT lprcClipRect,LPOLEINPLACEFRAMEINFO lpFrameInfo);
      HRESULT (WINAPI *Scroll)(IOleInPlaceSiteEx *This,SIZE scrollExtant);
      HRESULT (WINAPI *OnUIDeactivate)(IOleInPlaceSiteEx *This,WINBOOL fUndoable);
      HRESULT (WINAPI *OnInPlaceDeactivate)(IOleInPlaceSiteEx *This);
      HRESULT (WINAPI *DiscardUndoState)(IOleInPlaceSiteEx *This);
      HRESULT (WINAPI *DeactivateAndUndo)(IOleInPlaceSiteEx *This);
      HRESULT (WINAPI *OnPosRectChange)(IOleInPlaceSiteEx *This,LPCRECT lprcPosRect);
      HRESULT (WINAPI *OnInPlaceActivateEx)(IOleInPlaceSiteEx *This,WINBOOL *pfNoRedraw,DWORD dwFlags);
      HRESULT (WINAPI *OnInPlaceDeactivateEx)(IOleInPlaceSiteEx *This,WINBOOL fNoRedraw);
      HRESULT (WINAPI *RequestUIActivate)(IOleInPlaceSiteEx *This);
    END_INTERFACE
  } IOleInPlaceSiteExVtbl;
  struct IOleInPlaceSiteEx {
    CONST_VTBL struct IOleInPlaceSiteExVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IOleInPlaceSiteEx_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IOleInPlaceSiteEx_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IOleInPlaceSiteEx_Release(This) (This)->lpVtbl->Release(This)
#define IOleInPlaceSiteEx_GetWindow(This,phwnd) (This)->lpVtbl->GetWindow(This,phwnd)
#define IOleInPlaceSiteEx_ContextSensitiveHelp(This,fEnterMode) (This)->lpVtbl->ContextSensitiveHelp(This,fEnterMode)
#define IOleInPlaceSiteEx_CanInPlaceActivate(This) (This)->lpVtbl->CanInPlaceActivate(This)
#define IOleInPlaceSiteEx_OnInPlaceActivate(This) (This)->lpVtbl->OnInPlaceActivate(This)
#define IOleInPlaceSiteEx_OnUIActivate(This) (This)->lpVtbl->OnUIActivate(This)
#define IOleInPlaceSiteEx_GetWindowContext(This,ppFrame,ppDoc,lprcPosRect,lprcClipRect,lpFrameInfo) (This)->lpVtbl->GetWindowContext(This,ppFrame,ppDoc,lprcPosRect,lprcClipRect,lpFrameInfo)
#define IOleInPlaceSiteEx_Scroll(This,scrollExtant) (This)->lpVtbl->Scroll(This,scrollExtant)
#define IOleInPlaceSiteEx_OnUIDeactivate(This,fUndoable) (This)->lpVtbl->OnUIDeactivate(This,fUndoable)
#define IOleInPlaceSiteEx_OnInPlaceDeactivate(This) (This)->lpVtbl->OnInPlaceDeactivate(This)
#define IOleInPlaceSiteEx_DiscardUndoState(This) (This)->lpVtbl->DiscardUndoState(This)
#define IOleInPlaceSiteEx_DeactivateAndUndo(This) (This)->lpVtbl->DeactivateAndUndo(This)
#define IOleInPlaceSiteEx_OnPosRectChange(This,lprcPosRect) (This)->lpVtbl->OnPosRectChange(This,lprcPosRect)
#define IOleInPlaceSiteEx_OnInPlaceActivateEx(This,pfNoRedraw,dwFlags) (This)->lpVtbl->OnInPlaceActivateEx(This,pfNoRedraw,dwFlags)
#define IOleInPlaceSiteEx_OnInPlaceDeactivateEx(This,fNoRedraw) (This)->lpVtbl->OnInPlaceDeactivateEx(This,fNoRedraw)
#define IOleInPlaceSiteEx_RequestUIActivate(This) (This)->lpVtbl->RequestUIActivate(This)
#endif
#endif
  HRESULT WINAPI IOleInPlaceSiteEx_OnInPlaceActivateEx_Proxy(IOleInPlaceSiteEx *This,WINBOOL *pfNoRedraw,DWORD dwFlags);
  void __RPC_STUB IOleInPlaceSiteEx_OnInPlaceActivateEx_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteEx_OnInPlaceDeactivateEx_Proxy(IOleInPlaceSiteEx *This,WINBOOL fNoRedraw);
  void __RPC_STUB IOleInPlaceSiteEx_OnInPlaceDeactivateEx_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteEx_RequestUIActivate_Proxy(IOleInPlaceSiteEx *This);
  void __RPC_STUB IOleInPlaceSiteEx_RequestUIActivate_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IOleInPlaceSiteWindowless_INTERFACE_DEFINED__
#define __IOleInPlaceSiteWindowless_INTERFACE_DEFINED__
  typedef IOleInPlaceSiteWindowless *LPOLEINPLACESITEWINDOWLESS;

  typedef enum tagOLEDCFLAGS {
    OLEDC_NODRAW = 0x1,OLEDC_PAINTBKGND = 0x2,OLEDC_OFFSCREEN = 0x4
  } OLEDCFLAGS;

  EXTERN_C const IID IID_IOleInPlaceSiteWindowless;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IOleInPlaceSiteWindowless : public IOleInPlaceSiteEx {
  public:
    virtual HRESULT WINAPI CanWindowlessActivate(void) = 0;
    virtual HRESULT WINAPI GetCapture(void) = 0;
    virtual HRESULT WINAPI SetCapture(WINBOOL fCapture) = 0;
    virtual HRESULT WINAPI GetFocus(void) = 0;
    virtual HRESULT WINAPI SetFocus(WINBOOL fFocus) = 0;
    virtual HRESULT WINAPI GetDC(LPCRECT pRect,DWORD grfFlags,HDC *phDC) = 0;
    virtual HRESULT WINAPI ReleaseDC(HDC hDC) = 0;
    virtual HRESULT WINAPI InvalidateRect(LPCRECT pRect,WINBOOL fErase) = 0;
    virtual HRESULT WINAPI InvalidateRgn(HRGN hRGN,WINBOOL fErase) = 0;
    virtual HRESULT WINAPI ScrollRect(INT dx,INT dy,LPCRECT pRectScroll,LPCRECT pRectClip) = 0;
    virtual HRESULT WINAPI AdjustRect(LPRECT prc) = 0;
    virtual HRESULT WINAPI OnDefWindowMessage(UINT msg,WPARAM wParam,LPARAM lParam,LRESULT *plResult) = 0;
  };
#else
  typedef struct IOleInPlaceSiteWindowlessVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IOleInPlaceSiteWindowless *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IOleInPlaceSiteWindowless *This);
      ULONG (WINAPI *Release)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *GetWindow)(IOleInPlaceSiteWindowless *This,HWND *phwnd);
      HRESULT (WINAPI *ContextSensitiveHelp)(IOleInPlaceSiteWindowless *This,WINBOOL fEnterMode);
      HRESULT (WINAPI *CanInPlaceActivate)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *OnInPlaceActivate)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *OnUIActivate)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *GetWindowContext)(IOleInPlaceSiteWindowless *This,IOleInPlaceFrame **ppFrame,IOleInPlaceUIWindow **ppDoc,LPRECT lprcPosRect,LPRECT lprcClipRect,LPOLEINPLACEFRAMEINFO lpFrameInfo);
      HRESULT (WINAPI *Scroll)(IOleInPlaceSiteWindowless *This,SIZE scrollExtant);
      HRESULT (WINAPI *OnUIDeactivate)(IOleInPlaceSiteWindowless *This,WINBOOL fUndoable);
      HRESULT (WINAPI *OnInPlaceDeactivate)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *DiscardUndoState)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *DeactivateAndUndo)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *OnPosRectChange)(IOleInPlaceSiteWindowless *This,LPCRECT lprcPosRect);
      HRESULT (WINAPI *OnInPlaceActivateEx)(IOleInPlaceSiteWindowless *This,WINBOOL *pfNoRedraw,DWORD dwFlags);
      HRESULT (WINAPI *OnInPlaceDeactivateEx)(IOleInPlaceSiteWindowless *This,WINBOOL fNoRedraw);
      HRESULT (WINAPI *RequestUIActivate)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *CanWindowlessActivate)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *GetCapture)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *SetCapture)(IOleInPlaceSiteWindowless *This,WINBOOL fCapture);
      HRESULT (WINAPI *GetFocus)(IOleInPlaceSiteWindowless *This);
      HRESULT (WINAPI *SetFocus)(IOleInPlaceSiteWindowless *This,WINBOOL fFocus);
      HRESULT (WINAPI *GetDC)(IOleInPlaceSiteWindowless *This,LPCRECT pRect,DWORD grfFlags,HDC *phDC);
      HRESULT (WINAPI *ReleaseDC)(IOleInPlaceSiteWindowless *This,HDC hDC);
      HRESULT (WINAPI *InvalidateRect)(IOleInPlaceSiteWindowless *This,LPCRECT pRect,WINBOOL fErase);
      HRESULT (WINAPI *InvalidateRgn)(IOleInPlaceSiteWindowless *This,HRGN hRGN,WINBOOL fErase);
      HRESULT (WINAPI *ScrollRect)(IOleInPlaceSiteWindowless *This,INT dx,INT dy,LPCRECT pRectScroll,LPCRECT pRectClip);
      HRESULT (WINAPI *AdjustRect)(IOleInPlaceSiteWindowless *This,LPRECT prc);
      HRESULT (WINAPI *OnDefWindowMessage)(IOleInPlaceSiteWindowless *This,UINT msg,WPARAM wParam,LPARAM lParam,LRESULT *plResult);
    END_INTERFACE
  } IOleInPlaceSiteWindowlessVtbl;
  struct IOleInPlaceSiteWindowless {
    CONST_VTBL struct IOleInPlaceSiteWindowlessVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IOleInPlaceSiteWindowless_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IOleInPlaceSiteWindowless_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IOleInPlaceSiteWindowless_Release(This) (This)->lpVtbl->Release(This)
#define IOleInPlaceSiteWindowless_GetWindow(This,phwnd) (This)->lpVtbl->GetWindow(This,phwnd)
#define IOleInPlaceSiteWindowless_ContextSensitiveHelp(This,fEnterMode) (This)->lpVtbl->ContextSensitiveHelp(This,fEnterMode)
#define IOleInPlaceSiteWindowless_CanInPlaceActivate(This) (This)->lpVtbl->CanInPlaceActivate(This)
#define IOleInPlaceSiteWindowless_OnInPlaceActivate(This) (This)->lpVtbl->OnInPlaceActivate(This)
#define IOleInPlaceSiteWindowless_OnUIActivate(This) (This)->lpVtbl->OnUIActivate(This)
#define IOleInPlaceSiteWindowless_GetWindowContext(This,ppFrame,ppDoc,lprcPosRect,lprcClipRect,lpFrameInfo) (This)->lpVtbl->GetWindowContext(This,ppFrame,ppDoc,lprcPosRect,lprcClipRect,lpFrameInfo)
#define IOleInPlaceSiteWindowless_Scroll(This,scrollExtant) (This)->lpVtbl->Scroll(This,scrollExtant)
#define IOleInPlaceSiteWindowless_OnUIDeactivate(This,fUndoable) (This)->lpVtbl->OnUIDeactivate(This,fUndoable)
#define IOleInPlaceSiteWindowless_OnInPlaceDeactivate(This) (This)->lpVtbl->OnInPlaceDeactivate(This)
#define IOleInPlaceSiteWindowless_DiscardUndoState(This) (This)->lpVtbl->DiscardUndoState(This)
#define IOleInPlaceSiteWindowless_DeactivateAndUndo(This) (This)->lpVtbl->DeactivateAndUndo(This)
#define IOleInPlaceSiteWindowless_OnPosRectChange(This,lprcPosRect) (This)->lpVtbl->OnPosRectChange(This,lprcPosRect)
#define IOleInPlaceSiteWindowless_OnInPlaceActivateEx(This,pfNoRedraw,dwFlags) (This)->lpVtbl->OnInPlaceActivateEx(This,pfNoRedraw,dwFlags)
#define IOleInPlaceSiteWindowless_OnInPlaceDeactivateEx(This,fNoRedraw) (This)->lpVtbl->OnInPlaceDeactivateEx(This,fNoRedraw)
#define IOleInPlaceSiteWindowless_RequestUIActivate(This) (This)->lpVtbl->RequestUIActivate(This)
#define IOleInPlaceSiteWindowless_CanWindowlessActivate(This) (This)->lpVtbl->CanWindowlessActivate(This)
#define IOleInPlaceSiteWindowless_GetCapture(This) (This)->lpVtbl->GetCapture(This)
#define IOleInPlaceSiteWindowless_SetCapture(This,fCapture) (This)->lpVtbl->SetCapture(This,fCapture)
#define IOleInPlaceSiteWindowless_GetFocus(This) (This)->lpVtbl->GetFocus(This)
#define IOleInPlaceSiteWindowless_SetFocus(This,fFocus) (This)->lpVtbl->SetFocus(This,fFocus)
#define IOleInPlaceSiteWindowless_GetDC(This,pRect,grfFlags,phDC) (This)->lpVtbl->GetDC(This,pRect,grfFlags,phDC)
#define IOleInPlaceSiteWindowless_ReleaseDC(This,hDC) (This)->lpVtbl->ReleaseDC(This,hDC)
#define IOleInPlaceSiteWindowless_InvalidateRect(This,pRect,fErase) (This)->lpVtbl->InvalidateRect(This,pRect,fErase)
#define IOleInPlaceSiteWindowless_InvalidateRgn(This,hRGN,fErase) (This)->lpVtbl->InvalidateRgn(This,hRGN,fErase)
#define IOleInPlaceSiteWindowless_ScrollRect(This,dx,dy,pRectScroll,pRectClip) (This)->lpVtbl->ScrollRect(This,dx,dy,pRectScroll,pRectClip)
#define IOleInPlaceSiteWindowless_AdjustRect(This,prc) (This)->lpVtbl->AdjustRect(This,prc)
#define IOleInPlaceSiteWindowless_OnDefWindowMessage(This,msg,wParam,lParam,plResult) (This)->lpVtbl->OnDefWindowMessage(This,msg,wParam,lParam,plResult)
#endif
#endif
  HRESULT WINAPI IOleInPlaceSiteWindowless_CanWindowlessActivate_Proxy(IOleInPlaceSiteWindowless *This);
  void __RPC_STUB IOleInPlaceSiteWindowless_CanWindowlessActivate_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_GetCapture_Proxy(IOleInPlaceSiteWindowless *This);
  void __RPC_STUB IOleInPlaceSiteWindowless_GetCapture_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_SetCapture_Proxy(IOleInPlaceSiteWindowless *This,WINBOOL fCapture);
  void __RPC_STUB IOleInPlaceSiteWindowless_SetCapture_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_GetFocus_Proxy(IOleInPlaceSiteWindowless *This);
  void __RPC_STUB IOleInPlaceSiteWindowless_GetFocus_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_SetFocus_Proxy(IOleInPlaceSiteWindowless *This,WINBOOL fFocus);
  void __RPC_STUB IOleInPlaceSiteWindowless_SetFocus_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_GetDC_Proxy(IOleInPlaceSiteWindowless *This,LPCRECT pRect,DWORD grfFlags,HDC *phDC);
  void __RPC_STUB IOleInPlaceSiteWindowless_GetDC_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_ReleaseDC_Proxy(IOleInPlaceSiteWindowless *This,HDC hDC);
  void __RPC_STUB IOleInPlaceSiteWindowless_ReleaseDC_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_InvalidateRect_Proxy(IOleInPlaceSiteWindowless *This,LPCRECT pRect,WINBOOL fErase);
  void __RPC_STUB IOleInPlaceSiteWindowless_InvalidateRect_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_InvalidateRgn_Proxy(IOleInPlaceSiteWindowless *This,HRGN hRGN,WINBOOL fErase);
  void __RPC_STUB IOleInPlaceSiteWindowless_InvalidateRgn_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_ScrollRect_Proxy(IOleInPlaceSiteWindowless *This,INT dx,INT dy,LPCRECT pRectScroll,LPCRECT pRectClip);
  void __RPC_STUB IOleInPlaceSiteWindowless_ScrollRect_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_AdjustRect_Proxy(IOleInPlaceSiteWindowless *This,LPRECT prc);
  void __RPC_STUB IOleInPlaceSiteWindowless_AdjustRect_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleInPlaceSiteWindowless_OnDefWindowMessage_Proxy(IOleInPlaceSiteWindowless *This,UINT msg,WPARAM wParam,LPARAM lParam,LRESULT *plResult);
  void __RPC_STUB IOleInPlaceSiteWindowless_OnDefWindowMessage_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IViewObjectEx_INTERFACE_DEFINED__
#define __IViewObjectEx_INTERFACE_DEFINED__
  typedef IViewObjectEx *LPVIEWOBJECTEX;

  typedef enum tagVIEWSTATUS {
    VIEWSTATUS_OPAQUE = 1,VIEWSTATUS_SOLIDBKGND = 2,VIEWSTATUS_DVASPECTOPAQUE = 4,VIEWSTATUS_DVASPECTTRANSPARENT = 8,VIEWSTATUS_SURFACE = 16,
    VIEWSTATUS_3DSURFACE = 32
  } VIEWSTATUS;

  typedef enum tagHITRESULT {
    HITRESULT_OUTSIDE = 0,HITRESULT_TRANSPARENT = 1,HITRESULT_CLOSE = 2,HITRESULT_HIT = 3
  } HITRESULT;

  typedef enum tagDVASPECT2 {
    DVASPECT_OPAQUE = 16,DVASPECT_TRANSPARENT = 32
  } DVASPECT2;

  typedef struct tagExtentInfo {
    ULONG cb;
    DWORD dwExtentMode;
    SIZEL sizelProposed;
  } DVEXTENTINFO;

  typedef enum tagExtentMode {
    DVEXTENT_CONTENT = 0,DVEXTENT_INTEGRAL = DVEXTENT_CONTENT + 1
  } DVEXTENTMODE;

  typedef enum tagAspectInfoFlag {
    DVASPECTINFOFLAG_CANOPTIMIZE = 1
  } DVASPECTINFOFLAG;

  typedef struct tagAspectInfo {
    ULONG cb;
    DWORD dwFlags;
  } DVASPECTINFO;

  EXTERN_C const IID IID_IViewObjectEx;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IViewObjectEx : public IViewObject2 {
  public:
    virtual HRESULT WINAPI GetRect(DWORD dwAspect,LPRECTL pRect) = 0;
    virtual HRESULT WINAPI GetViewStatus(DWORD *pdwStatus) = 0;
    virtual HRESULT WINAPI QueryHitPoint(DWORD dwAspect,LPCRECT pRectBounds,POINT ptlLoc,LONG lCloseHint,DWORD *pHitResult) = 0;
    virtual HRESULT WINAPI QueryHitRect(DWORD dwAspect,LPCRECT pRectBounds,LPCRECT pRectLoc,LONG lCloseHint,DWORD *pHitResult) = 0;
    virtual HRESULT WINAPI GetNaturalExtent(DWORD dwAspect,LONG lindex,DVTARGETDEVICE *ptd,HDC hicTargetDev,DVEXTENTINFO *pExtentInfo,LPSIZEL pSizel) = 0;
  };
#else
  typedef struct IViewObjectExVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IViewObjectEx *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IViewObjectEx *This);
      ULONG (WINAPI *Release)(IViewObjectEx *This);
      HRESULT (WINAPI *Draw)(IViewObjectEx *This,DWORD dwDrawAspect,LONG lindex,void *pvAspect,DVTARGETDEVICE *ptd,HDC hdcTargetDev,HDC hdcDraw,LPCRECTL lprcBounds,LPCRECTL lprcWBounds,WINBOOL (WINAPI *pfnContinue)(ULONG_PTR dwContinue),ULONG_PTR dwContinue);
      HRESULT (WINAPI *GetColorSet)(IViewObjectEx *This,DWORD dwDrawAspect,LONG lindex,void *pvAspect,DVTARGETDEVICE *ptd,HDC hicTargetDev,LOGPALETTE **ppColorSet);
      HRESULT (WINAPI *Freeze)(IViewObjectEx *This,DWORD dwDrawAspect,LONG lindex,void *pvAspect,DWORD *pdwFreeze);
      HRESULT (WINAPI *Unfreeze)(IViewObjectEx *This,DWORD dwFreeze);
      HRESULT (WINAPI *SetAdvise)(IViewObjectEx *This,DWORD aspects,DWORD advf,IAdviseSink *pAdvSink);
      HRESULT (WINAPI *GetAdvise)(IViewObjectEx *This,DWORD *pAspects,DWORD *pAdvf,IAdviseSink **ppAdvSink);
      HRESULT (WINAPI *GetExtent)(IViewObjectEx *This,DWORD dwDrawAspect,LONG lindex,DVTARGETDEVICE *ptd,LPSIZEL lpsizel);
      HRESULT (WINAPI *GetRect)(IViewObjectEx *This,DWORD dwAspect,LPRECTL pRect);
      HRESULT (WINAPI *GetViewStatus)(IViewObjectEx *This,DWORD *pdwStatus);
      HRESULT (WINAPI *QueryHitPoint)(IViewObjectEx *This,DWORD dwAspect,LPCRECT pRectBounds,POINT ptlLoc,LONG lCloseHint,DWORD *pHitResult);
      HRESULT (WINAPI *QueryHitRect)(IViewObjectEx *This,DWORD dwAspect,LPCRECT pRectBounds,LPCRECT pRectLoc,LONG lCloseHint,DWORD *pHitResult);
      HRESULT (WINAPI *GetNaturalExtent)(IViewObjectEx *This,DWORD dwAspect,LONG lindex,DVTARGETDEVICE *ptd,HDC hicTargetDev,DVEXTENTINFO *pExtentInfo,LPSIZEL pSizel);
    END_INTERFACE
  } IViewObjectExVtbl;
  struct IViewObjectEx {
    CONST_VTBL struct IViewObjectExVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IViewObjectEx_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IViewObjectEx_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IViewObjectEx_Release(This) (This)->lpVtbl->Release(This)
#define IViewObjectEx_Draw(This,dwDrawAspect,lindex,pvAspect,ptd,hdcTargetDev,hdcDraw,lprcBounds,lprcWBounds,pfnContinue,dwContinue) (This)->lpVtbl->Draw(This,dwDrawAspect,lindex,pvAspect,ptd,hdcTargetDev,hdcDraw,lprcBounds,lprcWBounds,pfnContinue,dwContinue)
#define IViewObjectEx_GetColorSet(This,dwDrawAspect,lindex,pvAspect,ptd,hicTargetDev,ppColorSet) (This)->lpVtbl->GetColorSet(This,dwDrawAspect,lindex,pvAspect,ptd,hicTargetDev,ppColorSet)
#define IViewObjectEx_Freeze(This,dwDrawAspect,lindex,pvAspect,pdwFreeze) (This)->lpVtbl->Freeze(This,dwDrawAspect,lindex,pvAspect,pdwFreeze)
#define IViewObjectEx_Unfreeze(This,dwFreeze) (This)->lpVtbl->Unfreeze(This,dwFreeze)
#define IViewObjectEx_SetAdvise(This,aspects,advf,pAdvSink) (This)->lpVtbl->SetAdvise(This,aspects,advf,pAdvSink)
#define IViewObjectEx_GetAdvise(This,pAspects,pAdvf,ppAdvSink) (This)->lpVtbl->GetAdvise(This,pAspects,pAdvf,ppAdvSink)
#define IViewObjectEx_GetExtent(This,dwDrawAspect,lindex,ptd,lpsizel) (This)->lpVtbl->GetExtent(This,dwDrawAspect,lindex,ptd,lpsizel)
#define IViewObjectEx_GetRect(This,dwAspect,pRect) (This)->lpVtbl->GetRect(This,dwAspect,pRect)
#define IViewObjectEx_GetViewStatus(This,pdwStatus) (This)->lpVtbl->GetViewStatus(This,pdwStatus)
#define IViewObjectEx_QueryHitPoint(This,dwAspect,pRectBounds,ptlLoc,lCloseHint,pHitResult) (This)->lpVtbl->QueryHitPoint(This,dwAspect,pRectBounds,ptlLoc,lCloseHint,pHitResult)
#define IViewObjectEx_QueryHitRect(This,dwAspect,pRectBounds,pRectLoc,lCloseHint,pHitResult) (This)->lpVtbl->QueryHitRect(This,dwAspect,pRectBounds,pRectLoc,lCloseHint,pHitResult)
#define IViewObjectEx_GetNaturalExtent(This,dwAspect,lindex,ptd,hicTargetDev,pExtentInfo,pSizel) (This)->lpVtbl->GetNaturalExtent(This,dwAspect,lindex,ptd,hicTargetDev,pExtentInfo,pSizel)
#endif
#endif
  HRESULT WINAPI IViewObjectEx_GetRect_Proxy(IViewObjectEx *This,DWORD dwAspect,LPRECTL pRect);
  void __RPC_STUB IViewObjectEx_GetRect_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IViewObjectEx_GetViewStatus_Proxy(IViewObjectEx *This,DWORD *pdwStatus);
  void __RPC_STUB IViewObjectEx_GetViewStatus_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IViewObjectEx_QueryHitPoint_Proxy(IViewObjectEx *This,DWORD dwAspect,LPCRECT pRectBounds,POINT ptlLoc,LONG lCloseHint,DWORD *pHitResult);
  void __RPC_STUB IViewObjectEx_QueryHitPoint_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IViewObjectEx_QueryHitRect_Proxy(IViewObjectEx *This,DWORD dwAspect,LPCRECT pRectBounds,LPCRECT pRectLoc,LONG lCloseHint,DWORD *pHitResult);
  void __RPC_STUB IViewObjectEx_QueryHitRect_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IViewObjectEx_GetNaturalExtent_Proxy(IViewObjectEx *This,DWORD dwAspect,LONG lindex,DVTARGETDEVICE *ptd,HDC hicTargetDev,DVEXTENTINFO *pExtentInfo,LPSIZEL pSizel);
  void __RPC_STUB IViewObjectEx_GetNaturalExtent_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IOleUndoUnit_INTERFACE_DEFINED__
#define __IOleUndoUnit_INTERFACE_DEFINED__
  typedef IOleUndoUnit *LPOLEUNDOUNIT;

  EXTERN_C const IID IID_IOleUndoUnit;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IOleUndoUnit : public IUnknown {
  public:
    virtual HRESULT WINAPI Do(IOleUndoManager *pUndoManager) = 0;
    virtual HRESULT WINAPI GetDescription(BSTR *pBstr) = 0;
    virtual HRESULT WINAPI GetUnitType(CLSID *pClsid,LONG *plID) = 0;
    virtual HRESULT WINAPI OnNextAdd(void) = 0;
  };
#else
  typedef struct IOleUndoUnitVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IOleUndoUnit *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IOleUndoUnit *This);
      ULONG (WINAPI *Release)(IOleUndoUnit *This);
      HRESULT (WINAPI *Do)(IOleUndoUnit *This,IOleUndoManager *pUndoManager);
      HRESULT (WINAPI *GetDescription)(IOleUndoUnit *This,BSTR *pBstr);
      HRESULT (WINAPI *GetUnitType)(IOleUndoUnit *This,CLSID *pClsid,LONG *plID);
      HRESULT (WINAPI *OnNextAdd)(IOleUndoUnit *This);
    END_INTERFACE
  } IOleUndoUnitVtbl;
  struct IOleUndoUnit {
    CONST_VTBL struct IOleUndoUnitVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IOleUndoUnit_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IOleUndoUnit_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IOleUndoUnit_Release(This) (This)->lpVtbl->Release(This)
#define IOleUndoUnit_Do(This,pUndoManager) (This)->lpVtbl->Do(This,pUndoManager)
#define IOleUndoUnit_GetDescription(This,pBstr) (This)->lpVtbl->GetDescription(This,pBstr)
#define IOleUndoUnit_GetUnitType(This,pClsid,plID) (This)->lpVtbl->GetUnitType(This,pClsid,plID)
#define IOleUndoUnit_OnNextAdd(This) (This)->lpVtbl->OnNextAdd(This)
#endif
#endif
  HRESULT WINAPI IOleUndoUnit_Do_Proxy(IOleUndoUnit *This,IOleUndoManager *pUndoManager);
  void __RPC_STUB IOleUndoUnit_Do_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoUnit_GetDescription_Proxy(IOleUndoUnit *This,BSTR *pBstr);
  void __RPC_STUB IOleUndoUnit_GetDescription_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoUnit_GetUnitType_Proxy(IOleUndoUnit *This,CLSID *pClsid,LONG *plID);
  void __RPC_STUB IOleUndoUnit_GetUnitType_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoUnit_OnNextAdd_Proxy(IOleUndoUnit *This);
  void __RPC_STUB IOleUndoUnit_OnNextAdd_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IOleParentUndoUnit_INTERFACE_DEFINED__
#define __IOleParentUndoUnit_INTERFACE_DEFINED__
  typedef IOleParentUndoUnit *LPOLEPARENTUNDOUNIT;

  EXTERN_C const IID IID_IOleParentUndoUnit;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IOleParentUndoUnit : public IOleUndoUnit {
  public:
    virtual HRESULT WINAPI Open(IOleParentUndoUnit *pPUU) = 0;
    virtual HRESULT WINAPI Close(IOleParentUndoUnit *pPUU,WINBOOL fCommit) = 0;
    virtual HRESULT WINAPI Add(IOleUndoUnit *pUU) = 0;
    virtual HRESULT WINAPI FindUnit(IOleUndoUnit *pUU) = 0;
    virtual HRESULT WINAPI GetParentState(DWORD *pdwState) = 0;
  };
#else
  typedef struct IOleParentUndoUnitVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IOleParentUndoUnit *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IOleParentUndoUnit *This);
      ULONG (WINAPI *Release)(IOleParentUndoUnit *This);
      HRESULT (WINAPI *Do)(IOleParentUndoUnit *This,IOleUndoManager *pUndoManager);
      HRESULT (WINAPI *GetDescription)(IOleParentUndoUnit *This,BSTR *pBstr);
      HRESULT (WINAPI *GetUnitType)(IOleParentUndoUnit *This,CLSID *pClsid,LONG *plID);
      HRESULT (WINAPI *OnNextAdd)(IOleParentUndoUnit *This);
      HRESULT (WINAPI *Open)(IOleParentUndoUnit *This,IOleParentUndoUnit *pPUU);
      HRESULT (WINAPI *Close)(IOleParentUndoUnit *This,IOleParentUndoUnit *pPUU,WINBOOL fCommit);
      HRESULT (WINAPI *Add)(IOleParentUndoUnit *This,IOleUndoUnit *pUU);
      HRESULT (WINAPI *FindUnit)(IOleParentUndoUnit *This,IOleUndoUnit *pUU);
      HRESULT (WINAPI *GetParentState)(IOleParentUndoUnit *This,DWORD *pdwState);
    END_INTERFACE
  } IOleParentUndoUnitVtbl;
  struct IOleParentUndoUnit {
    CONST_VTBL struct IOleParentUndoUnitVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IOleParentUndoUnit_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IOleParentUndoUnit_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IOleParentUndoUnit_Release(This) (This)->lpVtbl->Release(This)
#define IOleParentUndoUnit_Do(This,pUndoManager) (This)->lpVtbl->Do(This,pUndoManager)
#define IOleParentUndoUnit_GetDescription(This,pBstr) (This)->lpVtbl->GetDescription(This,pBstr)
#define IOleParentUndoUnit_GetUnitType(This,pClsid,plID) (This)->lpVtbl->GetUnitType(This,pClsid,plID)
#define IOleParentUndoUnit_OnNextAdd(This) (This)->lpVtbl->OnNextAdd(This)
#define IOleParentUndoUnit_Open(This,pPUU) (This)->lpVtbl->Open(This,pPUU)
#define IOleParentUndoUnit_Close(This,pPUU,fCommit) (This)->lpVtbl->Close(This,pPUU,fCommit)
#define IOleParentUndoUnit_Add(This,pUU) (This)->lpVtbl->Add(This,pUU)
#define IOleParentUndoUnit_FindUnit(This,pUU) (This)->lpVtbl->FindUnit(This,pUU)
#define IOleParentUndoUnit_GetParentState(This,pdwState) (This)->lpVtbl->GetParentState(This,pdwState)
#endif
#endif
  HRESULT WINAPI IOleParentUndoUnit_Open_Proxy(IOleParentUndoUnit *This,IOleParentUndoUnit *pPUU);
  void __RPC_STUB IOleParentUndoUnit_Open_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleParentUndoUnit_Close_Proxy(IOleParentUndoUnit *This,IOleParentUndoUnit *pPUU,WINBOOL fCommit);
  void __RPC_STUB IOleParentUndoUnit_Close_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleParentUndoUnit_Add_Proxy(IOleParentUndoUnit *This,IOleUndoUnit *pUU);
  void __RPC_STUB IOleParentUndoUnit_Add_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleParentUndoUnit_FindUnit_Proxy(IOleParentUndoUnit *This,IOleUndoUnit *pUU);
  void __RPC_STUB IOleParentUndoUnit_FindUnit_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleParentUndoUnit_GetParentState_Proxy(IOleParentUndoUnit *This,DWORD *pdwState);
  void __RPC_STUB IOleParentUndoUnit_GetParentState_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IEnumOleUndoUnits_INTERFACE_DEFINED__
#define __IEnumOleUndoUnits_INTERFACE_DEFINED__
  typedef IEnumOleUndoUnits *LPENUMOLEUNDOUNITS;

  EXTERN_C const IID IID_IEnumOleUndoUnits;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IEnumOleUndoUnits : public IUnknown {
  public:
    virtual HRESULT WINAPI Next(ULONG cElt,IOleUndoUnit **rgElt,ULONG *pcEltFetched) = 0;
    virtual HRESULT WINAPI Skip(ULONG cElt) = 0;
    virtual HRESULT WINAPI Reset(void) = 0;
    virtual HRESULT WINAPI Clone(IEnumOleUndoUnits **ppEnum) = 0;
  };
#else
  typedef struct IEnumOleUndoUnitsVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IEnumOleUndoUnits *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IEnumOleUndoUnits *This);
      ULONG (WINAPI *Release)(IEnumOleUndoUnits *This);
      HRESULT (WINAPI *Next)(IEnumOleUndoUnits *This,ULONG cElt,IOleUndoUnit **rgElt,ULONG *pcEltFetched);
      HRESULT (WINAPI *Skip)(IEnumOleUndoUnits *This,ULONG cElt);
      HRESULT (WINAPI *Reset)(IEnumOleUndoUnits *This);
      HRESULT (WINAPI *Clone)(IEnumOleUndoUnits *This,IEnumOleUndoUnits **ppEnum);
    END_INTERFACE
  } IEnumOleUndoUnitsVtbl;
  struct IEnumOleUndoUnits {
    CONST_VTBL struct IEnumOleUndoUnitsVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IEnumOleUndoUnits_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IEnumOleUndoUnits_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IEnumOleUndoUnits_Release(This) (This)->lpVtbl->Release(This)
#define IEnumOleUndoUnits_Next(This,cElt,rgElt,pcEltFetched) (This)->lpVtbl->Next(This,cElt,rgElt,pcEltFetched)
#define IEnumOleUndoUnits_Skip(This,cElt) (This)->lpVtbl->Skip(This,cElt)
#define IEnumOleUndoUnits_Reset(This) (This)->lpVtbl->Reset(This)
#define IEnumOleUndoUnits_Clone(This,ppEnum) (This)->lpVtbl->Clone(This,ppEnum)
#endif
#endif
  HRESULT WINAPI IEnumOleUndoUnits_RemoteNext_Proxy(IEnumOleUndoUnits *This,ULONG cElt,IOleUndoUnit **rgElt,ULONG *pcEltFetched);
  void __RPC_STUB IEnumOleUndoUnits_RemoteNext_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IEnumOleUndoUnits_Skip_Proxy(IEnumOleUndoUnits *This,ULONG cElt);
  void __RPC_STUB IEnumOleUndoUnits_Skip_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IEnumOleUndoUnits_Reset_Proxy(IEnumOleUndoUnits *This);
  void __RPC_STUB IEnumOleUndoUnits_Reset_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IEnumOleUndoUnits_Clone_Proxy(IEnumOleUndoUnits *This,IEnumOleUndoUnits **ppEnum);
  void __RPC_STUB IEnumOleUndoUnits_Clone_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IOleUndoManager_INTERFACE_DEFINED__
#define __IOleUndoManager_INTERFACE_DEFINED__
#define SID_SOleUndoManager IID_IOleUndoManager
  typedef IOleUndoManager *LPOLEUNDOMANAGER;

  EXTERN_C const IID IID_IOleUndoManager;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IOleUndoManager : public IUnknown {
  public:
    virtual HRESULT WINAPI Open(IOleParentUndoUnit *pPUU) = 0;
    virtual HRESULT WINAPI Close(IOleParentUndoUnit *pPUU,WINBOOL fCommit) = 0;
    virtual HRESULT WINAPI Add(IOleUndoUnit *pUU) = 0;
    virtual HRESULT WINAPI GetOpenParentState(DWORD *pdwState) = 0;
    virtual HRESULT WINAPI DiscardFrom(IOleUndoUnit *pUU) = 0;
    virtual HRESULT WINAPI UndoTo(IOleUndoUnit *pUU) = 0;
    virtual HRESULT WINAPI RedoTo(IOleUndoUnit *pUU) = 0;
    virtual HRESULT WINAPI EnumUndoable(IEnumOleUndoUnits **ppEnum) = 0;
    virtual HRESULT WINAPI EnumRedoable(IEnumOleUndoUnits **ppEnum) = 0;
    virtual HRESULT WINAPI GetLastUndoDescription(BSTR *pBstr) = 0;
    virtual HRESULT WINAPI GetLastRedoDescription(BSTR *pBstr) = 0;
    virtual HRESULT WINAPI Enable(WINBOOL fEnable) = 0;
  };
#else
  typedef struct IOleUndoManagerVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IOleUndoManager *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IOleUndoManager *This);
      ULONG (WINAPI *Release)(IOleUndoManager *This);
      HRESULT (WINAPI *Open)(IOleUndoManager *This,IOleParentUndoUnit *pPUU);
      HRESULT (WINAPI *Close)(IOleUndoManager *This,IOleParentUndoUnit *pPUU,WINBOOL fCommit);
      HRESULT (WINAPI *Add)(IOleUndoManager *This,IOleUndoUnit *pUU);
      HRESULT (WINAPI *GetOpenParentState)(IOleUndoManager *This,DWORD *pdwState);
      HRESULT (WINAPI *DiscardFrom)(IOleUndoManager *This,IOleUndoUnit *pUU);
      HRESULT (WINAPI *UndoTo)(IOleUndoManager *This,IOleUndoUnit *pUU);
      HRESULT (WINAPI *RedoTo)(IOleUndoManager *This,IOleUndoUnit *pUU);
      HRESULT (WINAPI *EnumUndoable)(IOleUndoManager *This,IEnumOleUndoUnits **ppEnum);
      HRESULT (WINAPI *EnumRedoable)(IOleUndoManager *This,IEnumOleUndoUnits **ppEnum);
      HRESULT (WINAPI *GetLastUndoDescription)(IOleUndoManager *This,BSTR *pBstr);
      HRESULT (WINAPI *GetLastRedoDescription)(IOleUndoManager *This,BSTR *pBstr);
      HRESULT (WINAPI *Enable)(IOleUndoManager *This,WINBOOL fEnable);
    END_INTERFACE
  } IOleUndoManagerVtbl;
  struct IOleUndoManager {
    CONST_VTBL struct IOleUndoManagerVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IOleUndoManager_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IOleUndoManager_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IOleUndoManager_Release(This) (This)->lpVtbl->Release(This)
#define IOleUndoManager_Open(This,pPUU) (This)->lpVtbl->Open(This,pPUU)
#define IOleUndoManager_Close(This,pPUU,fCommit) (This)->lpVtbl->Close(This,pPUU,fCommit)
#define IOleUndoManager_Add(This,pUU) (This)->lpVtbl->Add(This,pUU)
#define IOleUndoManager_GetOpenParentState(This,pdwState) (This)->lpVtbl->GetOpenParentState(This,pdwState)
#define IOleUndoManager_DiscardFrom(This,pUU) (This)->lpVtbl->DiscardFrom(This,pUU)
#define IOleUndoManager_UndoTo(This,pUU) (This)->lpVtbl->UndoTo(This,pUU)
#define IOleUndoManager_RedoTo(This,pUU) (This)->lpVtbl->RedoTo(This,pUU)
#define IOleUndoManager_EnumUndoable(This,ppEnum) (This)->lpVtbl->EnumUndoable(This,ppEnum)
#define IOleUndoManager_EnumRedoable(This,ppEnum) (This)->lpVtbl->EnumRedoable(This,ppEnum)
#define IOleUndoManager_GetLastUndoDescription(This,pBstr) (This)->lpVtbl->GetLastUndoDescription(This,pBstr)
#define IOleUndoManager_GetLastRedoDescription(This,pBstr) (This)->lpVtbl->GetLastRedoDescription(This,pBstr)
#define IOleUndoManager_Enable(This,fEnable) (This)->lpVtbl->Enable(This,fEnable)
#endif
#endif
  HRESULT WINAPI IOleUndoManager_Open_Proxy(IOleUndoManager *This,IOleParentUndoUnit *pPUU);
  void __RPC_STUB IOleUndoManager_Open_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_Close_Proxy(IOleUndoManager *This,IOleParentUndoUnit *pPUU,WINBOOL fCommit);
  void __RPC_STUB IOleUndoManager_Close_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_Add_Proxy(IOleUndoManager *This,IOleUndoUnit *pUU);
  void __RPC_STUB IOleUndoManager_Add_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_GetOpenParentState_Proxy(IOleUndoManager *This,DWORD *pdwState);
  void __RPC_STUB IOleUndoManager_GetOpenParentState_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_DiscardFrom_Proxy(IOleUndoManager *This,IOleUndoUnit *pUU);
  void __RPC_STUB IOleUndoManager_DiscardFrom_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_UndoTo_Proxy(IOleUndoManager *This,IOleUndoUnit *pUU);
  void __RPC_STUB IOleUndoManager_UndoTo_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_RedoTo_Proxy(IOleUndoManager *This,IOleUndoUnit *pUU);
  void __RPC_STUB IOleUndoManager_RedoTo_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_EnumUndoable_Proxy(IOleUndoManager *This,IEnumOleUndoUnits **ppEnum);
  void __RPC_STUB IOleUndoManager_EnumUndoable_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_EnumRedoable_Proxy(IOleUndoManager *This,IEnumOleUndoUnits **ppEnum);
  void __RPC_STUB IOleUndoManager_EnumRedoable_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_GetLastUndoDescription_Proxy(IOleUndoManager *This,BSTR *pBstr);
  void __RPC_STUB IOleUndoManager_GetLastUndoDescription_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_GetLastRedoDescription_Proxy(IOleUndoManager *This,BSTR *pBstr);
  void __RPC_STUB IOleUndoManager_GetLastRedoDescription_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IOleUndoManager_Enable_Proxy(IOleUndoManager *This,WINBOOL fEnable);
  void __RPC_STUB IOleUndoManager_Enable_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPointerInactive_INTERFACE_DEFINED__
#define __IPointerInactive_INTERFACE_DEFINED__
  typedef IPointerInactive *LPPOINTERINACTIVE;

  typedef enum tagPOINTERINACTIVE {
    POINTERINACTIVE_ACTIVATEONENTRY = 1,POINTERINACTIVE_DEACTIVATEONLEAVE = 2,POINTERINACTIVE_ACTIVATEONDRAG = 4
  } POINTERINACTIVE;

  EXTERN_C const IID IID_IPointerInactive;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPointerInactive : public IUnknown {
  public:
    virtual HRESULT WINAPI GetActivationPolicy(DWORD *pdwPolicy) = 0;
    virtual HRESULT WINAPI OnInactiveMouseMove(LPCRECT pRectBounds,LONG x,LONG y,DWORD grfKeyState) = 0;
    virtual HRESULT WINAPI OnInactiveSetCursor(LPCRECT pRectBounds,LONG x,LONG y,DWORD dwMouseMsg,WINBOOL fSetAlways) = 0;
  };
#else
  typedef struct IPointerInactiveVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPointerInactive *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPointerInactive *This);
      ULONG (WINAPI *Release)(IPointerInactive *This);
      HRESULT (WINAPI *GetActivationPolicy)(IPointerInactive *This,DWORD *pdwPolicy);
      HRESULT (WINAPI *OnInactiveMouseMove)(IPointerInactive *This,LPCRECT pRectBounds,LONG x,LONG y,DWORD grfKeyState);
      HRESULT (WINAPI *OnInactiveSetCursor)(IPointerInactive *This,LPCRECT pRectBounds,LONG x,LONG y,DWORD dwMouseMsg,WINBOOL fSetAlways);
    END_INTERFACE
  } IPointerInactiveVtbl;
  struct IPointerInactive {
    CONST_VTBL struct IPointerInactiveVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPointerInactive_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPointerInactive_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPointerInactive_Release(This) (This)->lpVtbl->Release(This)
#define IPointerInactive_GetActivationPolicy(This,pdwPolicy) (This)->lpVtbl->GetActivationPolicy(This,pdwPolicy)
#define IPointerInactive_OnInactiveMouseMove(This,pRectBounds,x,y,grfKeyState) (This)->lpVtbl->OnInactiveMouseMove(This,pRectBounds,x,y,grfKeyState)
#define IPointerInactive_OnInactiveSetCursor(This,pRectBounds,x,y,dwMouseMsg,fSetAlways) (This)->lpVtbl->OnInactiveSetCursor(This,pRectBounds,x,y,dwMouseMsg,fSetAlways)
#endif
#endif
  HRESULT WINAPI IPointerInactive_GetActivationPolicy_Proxy(IPointerInactive *This,DWORD *pdwPolicy);
  void __RPC_STUB IPointerInactive_GetActivationPolicy_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPointerInactive_OnInactiveMouseMove_Proxy(IPointerInactive *This,LPCRECT pRectBounds,LONG x,LONG y,DWORD grfKeyState);
  void __RPC_STUB IPointerInactive_OnInactiveMouseMove_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPointerInactive_OnInactiveSetCursor_Proxy(IPointerInactive *This,LPCRECT pRectBounds,LONG x,LONG y,DWORD dwMouseMsg,WINBOOL fSetAlways);
  void __RPC_STUB IPointerInactive_OnInactiveSetCursor_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IObjectWithSite_INTERFACE_DEFINED__
#define __IObjectWithSite_INTERFACE_DEFINED__
  typedef IObjectWithSite *LPOBJECTWITHSITE;

  EXTERN_C const IID IID_IObjectWithSite;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IObjectWithSite : public IUnknown {
  public:
    virtual HRESULT WINAPI SetSite(IUnknown *pUnkSite) = 0;
    virtual HRESULT WINAPI GetSite(REFIID riid,void **ppvSite) = 0;
  };
#else
  typedef struct IObjectWithSiteVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IObjectWithSite *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IObjectWithSite *This);
      ULONG (WINAPI *Release)(IObjectWithSite *This);
      HRESULT (WINAPI *SetSite)(IObjectWithSite *This,IUnknown *pUnkSite);
      HRESULT (WINAPI *GetSite)(IObjectWithSite *This,REFIID riid,void **ppvSite);
    END_INTERFACE
  } IObjectWithSiteVtbl;
  struct IObjectWithSite {
    CONST_VTBL struct IObjectWithSiteVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IObjectWithSite_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IObjectWithSite_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IObjectWithSite_Release(This) (This)->lpVtbl->Release(This)
#define IObjectWithSite_SetSite(This,pUnkSite) (This)->lpVtbl->SetSite(This,pUnkSite)
#define IObjectWithSite_GetSite(This,riid,ppvSite) (This)->lpVtbl->GetSite(This,riid,ppvSite)
#endif
#endif
  HRESULT WINAPI IObjectWithSite_SetSite_Proxy(IObjectWithSite *This,IUnknown *pUnkSite);
  void __RPC_STUB IObjectWithSite_SetSite_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IObjectWithSite_GetSite_Proxy(IObjectWithSite *This,REFIID riid,void **ppvSite);
  void __RPC_STUB IObjectWithSite_GetSite_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPerPropertyBrowsing_INTERFACE_DEFINED__
#define __IPerPropertyBrowsing_INTERFACE_DEFINED__
  typedef IPerPropertyBrowsing *LPPERPROPERTYBROWSING;

  typedef struct tagCALPOLESTR {
    ULONG cElems;
    LPOLESTR *pElems;
  } CALPOLESTR;

  typedef struct tagCALPOLESTR *LPCALPOLESTR;

  typedef struct tagCADWORD {
    ULONG cElems;
    DWORD *pElems;
  } CADWORD;

  typedef struct tagCADWORD *LPCADWORD;

  EXTERN_C const IID IID_IPerPropertyBrowsing;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPerPropertyBrowsing : public IUnknown {
  public:
    virtual HRESULT WINAPI GetDisplayString(DISPID dispID,BSTR *pBstr) = 0;
    virtual HRESULT WINAPI MapPropertyToPage(DISPID dispID,CLSID *pClsid) = 0;
    virtual HRESULT WINAPI GetPredefinedStrings(DISPID dispID,CALPOLESTR *pCaStringsOut,CADWORD *pCaCookiesOut) = 0;
    virtual HRESULT WINAPI GetPredefinedValue(DISPID dispID,DWORD dwCookie,VARIANT *pVarOut) = 0;
  };
#else
  typedef struct IPerPropertyBrowsingVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPerPropertyBrowsing *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPerPropertyBrowsing *This);
      ULONG (WINAPI *Release)(IPerPropertyBrowsing *This);
      HRESULT (WINAPI *GetDisplayString)(IPerPropertyBrowsing *This,DISPID dispID,BSTR *pBstr);
      HRESULT (WINAPI *MapPropertyToPage)(IPerPropertyBrowsing *This,DISPID dispID,CLSID *pClsid);
      HRESULT (WINAPI *GetPredefinedStrings)(IPerPropertyBrowsing *This,DISPID dispID,CALPOLESTR *pCaStringsOut,CADWORD *pCaCookiesOut);
      HRESULT (WINAPI *GetPredefinedValue)(IPerPropertyBrowsing *This,DISPID dispID,DWORD dwCookie,VARIANT *pVarOut);
    END_INTERFACE
  } IPerPropertyBrowsingVtbl;
  struct IPerPropertyBrowsing {
    CONST_VTBL struct IPerPropertyBrowsingVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPerPropertyBrowsing_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPerPropertyBrowsing_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPerPropertyBrowsing_Release(This) (This)->lpVtbl->Release(This)
#define IPerPropertyBrowsing_GetDisplayString(This,dispID,pBstr) (This)->lpVtbl->GetDisplayString(This,dispID,pBstr)
#define IPerPropertyBrowsing_MapPropertyToPage(This,dispID,pClsid) (This)->lpVtbl->MapPropertyToPage(This,dispID,pClsid)
#define IPerPropertyBrowsing_GetPredefinedStrings(This,dispID,pCaStringsOut,pCaCookiesOut) (This)->lpVtbl->GetPredefinedStrings(This,dispID,pCaStringsOut,pCaCookiesOut)
#define IPerPropertyBrowsing_GetPredefinedValue(This,dispID,dwCookie,pVarOut) (This)->lpVtbl->GetPredefinedValue(This,dispID,dwCookie,pVarOut)
#endif
#endif
  HRESULT WINAPI IPerPropertyBrowsing_GetDisplayString_Proxy(IPerPropertyBrowsing *This,DISPID dispID,BSTR *pBstr);
  void __RPC_STUB IPerPropertyBrowsing_GetDisplayString_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPerPropertyBrowsing_MapPropertyToPage_Proxy(IPerPropertyBrowsing *This,DISPID dispID,CLSID *pClsid);
  void __RPC_STUB IPerPropertyBrowsing_MapPropertyToPage_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPerPropertyBrowsing_GetPredefinedStrings_Proxy(IPerPropertyBrowsing *This,DISPID dispID,CALPOLESTR *pCaStringsOut,CADWORD *pCaCookiesOut);
  void __RPC_STUB IPerPropertyBrowsing_GetPredefinedStrings_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPerPropertyBrowsing_GetPredefinedValue_Proxy(IPerPropertyBrowsing *This,DISPID dispID,DWORD dwCookie,VARIANT *pVarOut);
  void __RPC_STUB IPerPropertyBrowsing_GetPredefinedValue_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPropertyBag2_INTERFACE_DEFINED__
#define __IPropertyBag2_INTERFACE_DEFINED__
  typedef IPropertyBag2 *LPPROPERTYBAG2;

  typedef enum tagPROPBAG2_TYPE {
    PROPBAG2_TYPE_UNDEFINED = 0,PROPBAG2_TYPE_DATA = 1,PROPBAG2_TYPE_URL = 2,PROPBAG2_TYPE_OBJECT = 3,PROPBAG2_TYPE_STREAM = 4,
    PROPBAG2_TYPE_STORAGE = 5,PROPBAG2_TYPE_MONIKER = 6
  } PROPBAG2_TYPE;

  typedef struct tagPROPBAG2 {
    DWORD dwType;
    VARTYPE vt;
    CLIPFORMAT cfType;
    DWORD dwHint;
    LPOLESTR pstrName;
    CLSID clsid;
  } PROPBAG2;

  EXTERN_C const IID IID_IPropertyBag2;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPropertyBag2 : public IUnknown {
  public:
    virtual HRESULT WINAPI Read(ULONG cProperties,PROPBAG2 *pPropBag,IErrorLog *pErrLog,VARIANT *pvarValue,HRESULT *phrError) = 0;
    virtual HRESULT WINAPI Write(ULONG cProperties,PROPBAG2 *pPropBag,VARIANT *pvarValue) = 0;
    virtual HRESULT WINAPI CountProperties(ULONG *pcProperties) = 0;
    virtual HRESULT WINAPI GetPropertyInfo(ULONG iProperty,ULONG cProperties,PROPBAG2 *pPropBag,ULONG *pcProperties) = 0;
    virtual HRESULT WINAPI LoadObject(LPCOLESTR pstrName,DWORD dwHint,IUnknown *pUnkObject,IErrorLog *pErrLog) = 0;
  };
#else
  typedef struct IPropertyBag2Vtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPropertyBag2 *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPropertyBag2 *This);
      ULONG (WINAPI *Release)(IPropertyBag2 *This);
      HRESULT (WINAPI *Read)(IPropertyBag2 *This,ULONG cProperties,PROPBAG2 *pPropBag,IErrorLog *pErrLog,VARIANT *pvarValue,HRESULT *phrError);
      HRESULT (WINAPI *Write)(IPropertyBag2 *This,ULONG cProperties,PROPBAG2 *pPropBag,VARIANT *pvarValue);
      HRESULT (WINAPI *CountProperties)(IPropertyBag2 *This,ULONG *pcProperties);
      HRESULT (WINAPI *GetPropertyInfo)(IPropertyBag2 *This,ULONG iProperty,ULONG cProperties,PROPBAG2 *pPropBag,ULONG *pcProperties);
      HRESULT (WINAPI *LoadObject)(IPropertyBag2 *This,LPCOLESTR pstrName,DWORD dwHint,IUnknown *pUnkObject,IErrorLog *pErrLog);
    END_INTERFACE
  } IPropertyBag2Vtbl;
  struct IPropertyBag2 {
    CONST_VTBL struct IPropertyBag2Vtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPropertyBag2_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPropertyBag2_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPropertyBag2_Release(This) (This)->lpVtbl->Release(This)
#define IPropertyBag2_Read(This,cProperties,pPropBag,pErrLog,pvarValue,phrError) (This)->lpVtbl->Read(This,cProperties,pPropBag,pErrLog,pvarValue,phrError)
#define IPropertyBag2_Write(This,cProperties,pPropBag,pvarValue) (This)->lpVtbl->Write(This,cProperties,pPropBag,pvarValue)
#define IPropertyBag2_CountProperties(This,pcProperties) (This)->lpVtbl->CountProperties(This,pcProperties)
#define IPropertyBag2_GetPropertyInfo(This,iProperty,cProperties,pPropBag,pcProperties) (This)->lpVtbl->GetPropertyInfo(This,iProperty,cProperties,pPropBag,pcProperties)
#define IPropertyBag2_LoadObject(This,pstrName,dwHint,pUnkObject,pErrLog) (This)->lpVtbl->LoadObject(This,pstrName,dwHint,pUnkObject,pErrLog)
#endif
#endif
  HRESULT WINAPI IPropertyBag2_Read_Proxy(IPropertyBag2 *This,ULONG cProperties,PROPBAG2 *pPropBag,IErrorLog *pErrLog,VARIANT *pvarValue,HRESULT *phrError);
  void __RPC_STUB IPropertyBag2_Read_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyBag2_Write_Proxy(IPropertyBag2 *This,ULONG cProperties,PROPBAG2 *pPropBag,VARIANT *pvarValue);
  void __RPC_STUB IPropertyBag2_Write_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyBag2_CountProperties_Proxy(IPropertyBag2 *This,ULONG *pcProperties);
  void __RPC_STUB IPropertyBag2_CountProperties_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyBag2_GetPropertyInfo_Proxy(IPropertyBag2 *This,ULONG iProperty,ULONG cProperties,PROPBAG2 *pPropBag,ULONG *pcProperties);
  void __RPC_STUB IPropertyBag2_GetPropertyInfo_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPropertyBag2_LoadObject_Proxy(IPropertyBag2 *This,LPCOLESTR pstrName,DWORD dwHint,IUnknown *pUnkObject,IErrorLog *pErrLog);
  void __RPC_STUB IPropertyBag2_LoadObject_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IPersistPropertyBag2_INTERFACE_DEFINED__
#define __IPersistPropertyBag2_INTERFACE_DEFINED__
  typedef IPersistPropertyBag2 *LPPERSISTPROPERTYBAG2;

  EXTERN_C const IID IID_IPersistPropertyBag2;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IPersistPropertyBag2 : public IPersist {
  public:
    virtual HRESULT WINAPI InitNew(void) = 0;
    virtual HRESULT WINAPI Load(IPropertyBag2 *pPropBag,IErrorLog *pErrLog) = 0;
    virtual HRESULT WINAPI Save(IPropertyBag2 *pPropBag,WINBOOL fClearDirty,WINBOOL fSaveAllProperties) = 0;
    virtual HRESULT WINAPI IsDirty(void) = 0;
  };
#else
  typedef struct IPersistPropertyBag2Vtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IPersistPropertyBag2 *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IPersistPropertyBag2 *This);
      ULONG (WINAPI *Release)(IPersistPropertyBag2 *This);
      HRESULT (WINAPI *GetClassID)(IPersistPropertyBag2 *This,CLSID *pClassID);
      HRESULT (WINAPI *InitNew)(IPersistPropertyBag2 *This);
      HRESULT (WINAPI *Load)(IPersistPropertyBag2 *This,IPropertyBag2 *pPropBag,IErrorLog *pErrLog);
      HRESULT (WINAPI *Save)(IPersistPropertyBag2 *This,IPropertyBag2 *pPropBag,WINBOOL fClearDirty,WINBOOL fSaveAllProperties);
      HRESULT (WINAPI *IsDirty)(IPersistPropertyBag2 *This);
    END_INTERFACE
  } IPersistPropertyBag2Vtbl;
  struct IPersistPropertyBag2 {
    CONST_VTBL struct IPersistPropertyBag2Vtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IPersistPropertyBag2_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IPersistPropertyBag2_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IPersistPropertyBag2_Release(This) (This)->lpVtbl->Release(This)
#define IPersistPropertyBag2_GetClassID(This,pClassID) (This)->lpVtbl->GetClassID(This,pClassID)
#define IPersistPropertyBag2_InitNew(This) (This)->lpVtbl->InitNew(This)
#define IPersistPropertyBag2_Load(This,pPropBag,pErrLog) (This)->lpVtbl->Load(This,pPropBag,pErrLog)
#define IPersistPropertyBag2_Save(This,pPropBag,fClearDirty,fSaveAllProperties) (This)->lpVtbl->Save(This,pPropBag,fClearDirty,fSaveAllProperties)
#define IPersistPropertyBag2_IsDirty(This) (This)->lpVtbl->IsDirty(This)
#endif
#endif
  HRESULT WINAPI IPersistPropertyBag2_InitNew_Proxy(IPersistPropertyBag2 *This);
  void __RPC_STUB IPersistPropertyBag2_InitNew_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistPropertyBag2_Load_Proxy(IPersistPropertyBag2 *This,IPropertyBag2 *pPropBag,IErrorLog *pErrLog);
  void __RPC_STUB IPersistPropertyBag2_Load_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistPropertyBag2_Save_Proxy(IPersistPropertyBag2 *This,IPropertyBag2 *pPropBag,WINBOOL fClearDirty,WINBOOL fSaveAllProperties);
  void __RPC_STUB IPersistPropertyBag2_Save_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IPersistPropertyBag2_IsDirty_Proxy(IPersistPropertyBag2 *This);
  void __RPC_STUB IPersistPropertyBag2_IsDirty_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IAdviseSinkEx_INTERFACE_DEFINED__
#define __IAdviseSinkEx_INTERFACE_DEFINED__
  typedef IAdviseSinkEx *LPADVISESINKEX;

  EXTERN_C const IID IID_IAdviseSinkEx;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IAdviseSinkEx : public IAdviseSink {
  public:
    virtual void WINAPI OnViewStatusChange(DWORD dwViewStatus) = 0;
  };
#else
  typedef struct IAdviseSinkExVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IAdviseSinkEx *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IAdviseSinkEx *This);
      ULONG (WINAPI *Release)(IAdviseSinkEx *This);
      void (WINAPI *OnDataChange)(IAdviseSinkEx *This,FORMATETC *pFormatetc,STGMEDIUM *pStgmed);
      void (WINAPI *OnViewChange)(IAdviseSinkEx *This,DWORD dwAspect,LONG lindex);
      void (WINAPI *OnRename)(IAdviseSinkEx *This,IMoniker *pmk);
      void (WINAPI *OnSave)(IAdviseSinkEx *This);
      void (WINAPI *OnClose)(IAdviseSinkEx *This);
      void (WINAPI *OnViewStatusChange)(IAdviseSinkEx *This,DWORD dwViewStatus);
    END_INTERFACE
  } IAdviseSinkExVtbl;
  struct IAdviseSinkEx {
    CONST_VTBL struct IAdviseSinkExVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IAdviseSinkEx_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IAdviseSinkEx_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IAdviseSinkEx_Release(This) (This)->lpVtbl->Release(This)
#define IAdviseSinkEx_OnDataChange(This,pFormatetc,pStgmed) (This)->lpVtbl->OnDataChange(This,pFormatetc,pStgmed)
#define IAdviseSinkEx_OnViewChange(This,dwAspect,lindex) (This)->lpVtbl->OnViewChange(This,dwAspect,lindex)
#define IAdviseSinkEx_OnRename(This,pmk) (This)->lpVtbl->OnRename(This,pmk)
#define IAdviseSinkEx_OnSave(This) (This)->lpVtbl->OnSave(This)
#define IAdviseSinkEx_OnClose(This) (This)->lpVtbl->OnClose(This)
#define IAdviseSinkEx_OnViewStatusChange(This,dwViewStatus) (This)->lpVtbl->OnViewStatusChange(This,dwViewStatus)
#endif
#endif
  HRESULT WINAPI IAdviseSinkEx_RemoteOnViewStatusChange_Proxy(IAdviseSinkEx *This,DWORD dwViewStatus);
  void __RPC_STUB IAdviseSinkEx_RemoteOnViewStatusChange_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __IQuickActivate_INTERFACE_DEFINED__
#define __IQuickActivate_INTERFACE_DEFINED__
  typedef IQuickActivate *LPQUICKACTIVATE;

  typedef enum tagQACONTAINERFLAGS {
    QACONTAINER_SHOWHATCHING = 0x1,QACONTAINER_SHOWGRABHANDLES = 0x2,QACONTAINER_USERMODE = 0x4,QACONTAINER_DISPLAYASDEFAULT = 0x8,
    QACONTAINER_UIDEAD = 0x10,QACONTAINER_AUTOCLIP = 0x20,QACONTAINER_MESSAGEREFLECT = 0x40,QACONTAINER_SUPPORTSMNEMONICS = 0x80
  } QACONTAINERFLAGS;

  typedef DWORD OLE_COLOR;

  typedef struct tagQACONTAINER {
    ULONG cbSize;
    IOleClientSite *pClientSite;
    IAdviseSinkEx *pAdviseSink;
    IPropertyNotifySink *pPropertyNotifySink;
    IUnknown *pUnkEventSink;
    DWORD dwAmbientFlags;
    OLE_COLOR colorFore;
    OLE_COLOR colorBack;
    IFont *pFont;
    IOleUndoManager *pUndoMgr;
    DWORD dwAppearance;
    LONG lcid;
    HPALETTE hpal;
    IBindHost *pBindHost;
    IOleControlSite *pOleControlSite;
    IServiceProvider *pServiceProvider;
  } QACONTAINER;

  typedef struct tagQACONTROL {
    ULONG cbSize;
    DWORD dwMiscStatus;
    DWORD dwViewStatus;
    DWORD dwEventCookie;
    DWORD dwPropNotifyCookie;
    DWORD dwPointerActivationPolicy;
  } QACONTROL;

  EXTERN_C const IID IID_IQuickActivate;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct IQuickActivate : public IUnknown {
  public:
    virtual HRESULT WINAPI QuickActivate(QACONTAINER *pQaContainer,QACONTROL *pQaControl) = 0;
    virtual HRESULT WINAPI SetContentExtent(LPSIZEL pSizel) = 0;
    virtual HRESULT WINAPI GetContentExtent(LPSIZEL pSizel) = 0;
  };
#else
  typedef struct IQuickActivateVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(IQuickActivate *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(IQuickActivate *This);
      ULONG (WINAPI *Release)(IQuickActivate *This);
      HRESULT (WINAPI *QuickActivate)(IQuickActivate *This,QACONTAINER *pQaContainer,QACONTROL *pQaControl);
      HRESULT (WINAPI *SetContentExtent)(IQuickActivate *This,LPSIZEL pSizel);
      HRESULT (WINAPI *GetContentExtent)(IQuickActivate *This,LPSIZEL pSizel);
    END_INTERFACE
  } IQuickActivateVtbl;
  struct IQuickActivate {
    CONST_VTBL struct IQuickActivateVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define IQuickActivate_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IQuickActivate_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IQuickActivate_Release(This) (This)->lpVtbl->Release(This)
#define IQuickActivate_QuickActivate(This,pQaContainer,pQaControl) (This)->lpVtbl->QuickActivate(This,pQaContainer,pQaControl)
#define IQuickActivate_SetContentExtent(This,pSizel) (This)->lpVtbl->SetContentExtent(This,pSizel)
#define IQuickActivate_GetContentExtent(This,pSizel) (This)->lpVtbl->GetContentExtent(This,pSizel)
#endif
#endif
  HRESULT WINAPI IQuickActivate_RemoteQuickActivate_Proxy(IQuickActivate *This,QACONTAINER *pQaContainer,QACONTROL *pQaControl);
  void __RPC_STUB IQuickActivate_RemoteQuickActivate_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IQuickActivate_SetContentExtent_Proxy(IQuickActivate *This,LPSIZEL pSizel);
  void __RPC_STUB IQuickActivate_SetContentExtent_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI IQuickActivate_GetContentExtent_Proxy(IQuickActivate *This,LPSIZEL pSizel);
  void __RPC_STUB IQuickActivate_GetContentExtent_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

  unsigned long __RPC_API BSTR_UserSize(unsigned long *,unsigned long,BSTR *);
  unsigned char *__RPC_API BSTR_UserMarshal(unsigned long *,unsigned char *,BSTR *);
  unsigned char *__RPC_API BSTR_UserUnmarshal(unsigned long *,unsigned char *,BSTR *);
  void __RPC_API BSTR_UserFree(unsigned long *,BSTR *);
  unsigned long __RPC_API CLIPFORMAT_UserSize(unsigned long *,unsigned long,CLIPFORMAT *);
  unsigned char *__RPC_API CLIPFORMAT_UserMarshal(unsigned long *,unsigned char *,CLIPFORMAT *);
  unsigned char *__RPC_API CLIPFORMAT_UserUnmarshal(unsigned long *,unsigned char *,CLIPFORMAT *);
  void __RPC_API CLIPFORMAT_UserFree(unsigned long *,CLIPFORMAT *);
  unsigned long __RPC_API HACCEL_UserSize(unsigned long *,unsigned long,HACCEL *);
  unsigned char *__RPC_API HACCEL_UserMarshal(unsigned long *,unsigned char *,HACCEL *);
  unsigned char *__RPC_API HACCEL_UserUnmarshal(unsigned long *,unsigned char *,HACCEL *);
  void __RPC_API HACCEL_UserFree(unsigned long *,HACCEL *);
  unsigned long __RPC_API HDC_UserSize(unsigned long *,unsigned long,HDC *);
  unsigned char *__RPC_API HDC_UserMarshal(unsigned long *,unsigned char *,HDC *);
  unsigned char *__RPC_API HDC_UserUnmarshal(unsigned long *,unsigned char *,HDC *);
  void __RPC_API HDC_UserFree(unsigned long *,HDC *);
  unsigned long __RPC_API HFONT_UserSize(unsigned long *,unsigned long,HFONT *);
  unsigned char *__RPC_API HFONT_UserMarshal(unsigned long *,unsigned char *,HFONT *);
  unsigned char *__RPC_API HFONT_UserUnmarshal(unsigned long *,unsigned char *,HFONT *);
  void __RPC_API HFONT_UserFree(unsigned long *,HFONT *);
  unsigned long __RPC_API HPALETTE_UserSize(unsigned long *,unsigned long,HPALETTE *);
  unsigned char *__RPC_API HPALETTE_UserMarshal(unsigned long *,unsigned char *,HPALETTE *);
  unsigned char *__RPC_API HPALETTE_UserUnmarshal(unsigned long *,unsigned char *,HPALETTE *);
  void __RPC_API HPALETTE_UserFree(unsigned long *,HPALETTE *);
  unsigned long __RPC_API HWND_UserSize(unsigned long *,unsigned long,HWND *);
  unsigned char *__RPC_API HWND_UserMarshal(unsigned long *,unsigned char *,HWND *);
  unsigned char *__RPC_API HWND_UserUnmarshal(unsigned long *,unsigned char *,HWND *);
  void __RPC_API HWND_UserFree(unsigned long *,HWND *);
  unsigned long __RPC_API VARIANT_UserSize(unsigned long *,unsigned long,VARIANT *);
  unsigned char *__RPC_API VARIANT_UserMarshal(unsigned long *,unsigned char *,VARIANT *);
  unsigned char *__RPC_API VARIANT_UserUnmarshal(unsigned long *,unsigned char *,VARIANT *);
  void __RPC_API VARIANT_UserFree(unsigned long *,VARIANT *);

  HRESULT WINAPI IEnumConnections_Next_Proxy(IEnumConnections *This,ULONG cConnections,LPCONNECTDATA rgcd,ULONG *pcFetched);
  HRESULT WINAPI IEnumConnections_Next_Stub(IEnumConnections *This,ULONG cConnections,LPCONNECTDATA rgcd,ULONG *pcFetched);
  HRESULT WINAPI IEnumConnectionPoints_Next_Proxy(IEnumConnectionPoints *This,ULONG cConnections,LPCONNECTIONPOINT *ppCP,ULONG *pcFetched);
  HRESULT WINAPI IEnumConnectionPoints_Next_Stub(IEnumConnectionPoints *This,ULONG cConnections,LPCONNECTIONPOINT *ppCP,ULONG *pcFetched);
  HRESULT WINAPI IClassFactory2_CreateInstanceLic_Proxy(IClassFactory2 *This,IUnknown *pUnkOuter,IUnknown *pUnkReserved,REFIID riid,BSTR bstrKey,PVOID *ppvObj);
  HRESULT WINAPI IClassFactory2_CreateInstanceLic_Stub(IClassFactory2 *This,REFIID riid,BSTR bstrKey,IUnknown **ppvObj);
  HRESULT WINAPI IPersistMemory_Load_Proxy(IPersistMemory *This,LPVOID pMem,ULONG cbSize);
  HRESULT WINAPI IPersistMemory_Load_Stub(IPersistMemory *This,BYTE *pMem,ULONG cbSize);
  HRESULT WINAPI IPersistMemory_Save_Proxy(IPersistMemory *This,LPVOID pMem,WINBOOL fClearDirty,ULONG cbSize);
  HRESULT WINAPI IPersistMemory_Save_Stub(IPersistMemory *This,BYTE *pMem,WINBOOL fClearDirty,ULONG cbSize);
  HRESULT WINAPI IEnumOleUndoUnits_Next_Proxy(IEnumOleUndoUnits *This,ULONG cElt,IOleUndoUnit **rgElt,ULONG *pcEltFetched);
  HRESULT WINAPI IEnumOleUndoUnits_Next_Stub(IEnumOleUndoUnits *This,ULONG cElt,IOleUndoUnit **rgElt,ULONG *pcEltFetched);
  void WINAPI IAdviseSinkEx_OnViewStatusChange_Proxy(IAdviseSinkEx *This,DWORD dwViewStatus);
  HRESULT WINAPI IAdviseSinkEx_OnViewStatusChange_Stub(IAdviseSinkEx *This,DWORD dwViewStatus);
  HRESULT WINAPI IQuickActivate_QuickActivate_Proxy(IQuickActivate *This,QACONTAINER *pQaContainer,QACONTROL *pQaControl);
  HRESULT WINAPI IQuickActivate_QuickActivate_Stub(IQuickActivate *This,QACONTAINER *pQaContainer,QACONTROL *pQaControl);

#ifdef __cplusplus
}
#endif
#endif
