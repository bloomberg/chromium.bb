/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the w64 mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _ADSHLP_
#define _ADSHLP_

#ifdef __cplusplus
extern "C" {
#endif

  HRESULT WINAPI ADsGetObject(LPCWSTR lpszPathName,REFIID riid,VOID **ppObject);
  HRESULT WINAPI ADsBuildEnumerator(IADsContainer *pADsContainer,IEnumVARIANT **ppEnumVariant);
  HRESULT WINAPI ADsFreeEnumerator(IEnumVARIANT *pEnumVariant);
  HRESULT WINAPI ADsEnumerateNext(IEnumVARIANT *pEnumVariant,ULONG cElements,VARIANT *pvar,ULONG *pcElementsFetched);
  HRESULT WINAPI ADsBuildVarArrayStr(LPWSTR *lppPathNames,DWORD dwPathNames,VARIANT *pVar);
  HRESULT WINAPI ADsBuildVarArrayInt(LPDWORD lpdwObjectTypes,DWORD dwObjectTypes,VARIANT *pVar);
  HRESULT WINAPI ADsOpenObject(LPCWSTR lpszPathName,LPCWSTR lpszUserName,LPCWSTR lpszPassword,DWORD dwReserved,REFIID riid,void **ppObject);
  HRESULT WINAPI ADsGetLastError(LPDWORD lpError,LPWSTR lpErrorBuf,DWORD dwErrorBufLen,LPWSTR lpNameBuf,DWORD dwNameBufLen);
  VOID WINAPI ADsSetLastError(DWORD dwErr,LPCWSTR pszError,LPCWSTR pszProvider);
  VOID WINAPI ADsFreeAllErrorRecords(VOID);
  LPVOID WINAPI AllocADsMem(DWORD cb);
  WINBOOL WINAPI FreeADsMem(LPVOID pMem);
  LPVOID WINAPI ReallocADsMem(LPVOID pOldMem,DWORD cbOld,DWORD cbNew);
  LPWSTR WINAPI AllocADsStr(LPCWSTR pStr);
  WINBOOL WINAPI FreeADsStr(LPWSTR pStr);
  WINBOOL WINAPI ReallocADsStr(LPWSTR *ppStr,LPWSTR pStr);
  HRESULT WINAPI ADsEncodeBinaryData(PBYTE pbSrcData,DWORD dwSrcLen,LPWSTR *ppszDestData);
  HRESULT WINAPI ADsDecodeBinaryData (LPCWSTR szSrcData,PBYTE *ppbDestData,ULONG *pdwDestLen);
  HRESULT WINAPI PropVariantToAdsType(VARIANT *pVariant,DWORD dwNumVariant,PADSVALUE *ppAdsValues,PDWORD pdwNumValues);
  HRESULT WINAPI AdsTypeToPropVariant(PADSVALUE pAdsValues,DWORD dwNumValues,VARIANT *pVariant);
  void WINAPI AdsFreeAdsValues(PADSVALUE pAdsValues,DWORD dwNumValues);
  HRESULT WINAPI BinarySDToSecurityDescriptor(PSECURITY_DESCRIPTOR pSecurityDescriptor,VARIANT *pVarsec,LPCWSTR pszServerName,LPCWSTR userName,LPCWSTR passWord,DWORD dwFlags);
  HRESULT WINAPI SecurityDescriptorToBinarySD(VARIANT vVarSecDes,PSECURITY_DESCRIPTOR *ppSecurityDescriptor,PDWORD pdwSDLength,LPCWSTR pszServerName,LPCWSTR userName,LPCWSTR passWord,DWORD dwFlags);

#define InitADsMem()
#define AssertADsMemLeaks()
#define DumpMemoryTracker()

#ifdef __cplusplus
}
#endif
#endif
