// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_
#pragma once

#include <objidl.h>
#include <shlobj.h>
#include <string>

#include "base/scoped_comptr_win.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace ui {

class DataObjectImpl : public DownloadFileObserver,
                       public IDataObject,
                       public IAsyncOperation {
 public:
  class Observer {
   public:
    virtual void OnWaitForData() = 0;
    virtual void OnDataObjectDisposed() = 0;
   protected:
    virtual ~Observer() { }
  };

  DataObjectImpl();

  // Accessors.
  void set_observer(Observer* observer) { observer_ = observer; }

  // DownloadFileObserver implementation:
  virtual void OnDownloadCompleted(const FilePath& file_path);
  virtual void OnDownloadAborted();

  // IDataObject implementation:
  HRESULT __stdcall GetData(FORMATETC* format_etc, STGMEDIUM* medium);
  HRESULT __stdcall GetDataHere(FORMATETC* format_etc, STGMEDIUM* medium);
  HRESULT __stdcall QueryGetData(FORMATETC* format_etc);
  HRESULT __stdcall GetCanonicalFormatEtc(
      FORMATETC* format_etc, FORMATETC* result);
  HRESULT __stdcall SetData(
      FORMATETC* format_etc, STGMEDIUM* medium, BOOL should_release);
  HRESULT __stdcall EnumFormatEtc(
      DWORD direction, IEnumFORMATETC** enumerator);
  HRESULT __stdcall DAdvise(FORMATETC* format_etc, DWORD advf,
                            IAdviseSink* sink, DWORD* connection);
  HRESULT __stdcall DUnadvise(DWORD connection);
  HRESULT __stdcall EnumDAdvise(IEnumSTATDATA** enumerator);

  // IAsyncOperation implementation:
  HRESULT __stdcall EndOperation(
      HRESULT result, IBindCtx* reserved, DWORD effects);
  HRESULT __stdcall GetAsyncMode(BOOL* is_op_async);
  HRESULT __stdcall InOperation(BOOL* in_async_op);
  HRESULT __stdcall SetAsyncMode(BOOL do_op_async);
  HRESULT __stdcall StartOperation(IBindCtx* reserved);

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

 private:
  // FormatEtcEnumerator only likes us for our StoredDataMap typedef.
  friend class FormatEtcEnumerator;
  friend class OSExchangeDataProviderWin;

  virtual ~DataObjectImpl();

  void StopDownloads();

  // Our internal representation of stored data & type info.
  struct StoredDataInfo {
    FORMATETC format_etc;
    STGMEDIUM* medium;
    bool owns_medium;
    bool in_delay_rendering;
    scoped_refptr<DownloadFileProvider> downloader;

    StoredDataInfo(CLIPFORMAT cf, STGMEDIUM* medium)
        : medium(medium),
          owns_medium(true),
          in_delay_rendering(false) {
      format_etc.cfFormat = cf;
      format_etc.dwAspect = DVASPECT_CONTENT;
      format_etc.lindex = -1;
      format_etc.ptd = NULL;
      format_etc.tymed = medium ? medium->tymed : TYMED_HGLOBAL;
    }

    StoredDataInfo(FORMATETC* format_etc, STGMEDIUM* medium)
        : format_etc(*format_etc),
          medium(medium),
          owns_medium(true),
          in_delay_rendering(false) {
    }

    ~StoredDataInfo() {
      if (owns_medium) {
        ReleaseStgMedium(medium);
        delete medium;
      }
      if (downloader.get())
        downloader->Stop();
    }
  };

  typedef std::vector<StoredDataInfo*> StoredData;
  StoredData contents_;

  ScopedComPtr<IDataObject> source_object_;

  bool is_aborting_;
  bool in_async_mode_;
  bool async_operation_started_;
  Observer* observer_;
};

class OSExchangeDataProviderWin : public OSExchangeData::Provider {
 public:
  // Returns true if source has plain text that is a valid url.
  static bool HasPlainTextURL(IDataObject* source);

  // Returns true if source has plain text that is a valid URL and sets url to
  // that url.
  static bool GetPlainTextURL(IDataObject* source, GURL* url);

  static DataObjectImpl* GetDataObjectImpl(const OSExchangeData& data);
  static IDataObject* GetIDataObject(const OSExchangeData& data);
  static IAsyncOperation* GetIAsyncOperation(const OSExchangeData& data);

  explicit OSExchangeDataProviderWin(IDataObject* source);
  OSExchangeDataProviderWin();

  virtual ~OSExchangeDataProviderWin();

  IDataObject* data_object() const { return data_.get(); }
  IAsyncOperation* async_operation() const { return data_.get(); }

  // OSExchangeData::Provider methods.
  virtual void SetString(const std::wstring& data);
  virtual void SetURL(const GURL& url, const std::wstring& title);
  virtual void SetFilename(const FilePath& path);
  virtual void SetPickledData(OSExchangeData::CustomFormat format,
                              const Pickle& data);
  virtual void SetFileContents(const std::wstring& filename,
                               const std::string& file_contents);
  virtual void SetHtml(const std::wstring& html, const GURL& base_url);

  virtual bool GetString(std::wstring* data) const;
  virtual bool GetURLAndTitle(GURL* url, std::wstring* title) const;
  virtual bool GetFilename(FilePath* path) const;
  virtual bool GetPickledData(OSExchangeData::CustomFormat format,
                              Pickle* data) const;
  virtual bool GetFileContents(std::wstring* filename,
                               std::string* file_contents) const;
  virtual bool GetHtml(std::wstring* html, GURL* base_url) const;
  virtual bool HasString() const;
  virtual bool HasURL() const;
  virtual bool HasFile() const;
  virtual bool HasFileContents() const;
  virtual bool HasHtml() const;
  virtual bool HasCustomFormat(OSExchangeData::CustomFormat format) const;
  virtual void SetDownloadFileInfo(
      const OSExchangeData::DownloadFileInfo& download_info);

 private:
  scoped_refptr<DataObjectImpl> data_;
  ScopedComPtr<IDataObject> source_object_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderWin);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_
