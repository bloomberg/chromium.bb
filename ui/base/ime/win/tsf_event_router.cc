// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/tsf_event_router.h"

#include <msctf.h>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/win/scoped_comptr.h"
#include "base/win/metro.h"

namespace ui {

class TsfEventRouterImpl : public TsfEventRouter,
                           public ITfUIElementSink,
                           public ITfTextEditSink {
 public:
  TsfEventRouterImpl()
      : observer_(NULL),
        context_source_cookie_(TF_INVALID_COOKIE),
        ui_source_cookie_(TF_INVALID_COOKIE),
        ref_count_(0) {}

  virtual ~TsfEventRouterImpl() {}

  // ITfTextEditSink override.
  virtual ULONG STDMETHODCALLTYPE AddRef() OVERRIDE {
    return InterlockedIncrement(&ref_count_);
  }

  // ITfTextEditSink override.
  virtual ULONG STDMETHODCALLTYPE Release() OVERRIDE {
    const LONG count = InterlockedDecrement(&ref_count_);
    if (!count) {
      delete this;
      return 0;
    }
    return static_cast<ULONG>(count);
  }

  // ITfTextEditSink override.
  virtual STDMETHODIMP QueryInterface(REFIID iid, void** result) OVERRIDE {
    if (!result)
      return E_INVALIDARG;
    if (iid == IID_IUnknown || iid == IID_ITfTextEditSink) {
      *result = static_cast<ITfTextEditSink*>(this);
    } else if (iid == IID_ITfUIElementSink) {
      *result = static_cast<ITfUIElementSink*>(this);
    } else {
      *result = NULL;
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }

  // ITfTextEditSink override.
  virtual STDMETHODIMP OnEndEdit(ITfContext* context,
                                 TfEditCookie read_only_cookie,
                                 ITfEditRecord* edit_record) OVERRIDE {
    if (!edit_record || !context)
      return E_INVALIDARG;
    if (!observer_)
      return S_OK;

    // |edit_record| can be used to obtain updated ranges in terms of text
    // contents and/or text attributes. Here we are interested only in text
    // update so we use TF_GTP_INCL_TEXT and check if there is any range which
    // contains updated text.
    base::win::ScopedComPtr<IEnumTfRanges> ranges;
    if (FAILED(edit_record->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT,
                                                      NULL,
                                                      0,
                                                      ranges.Receive()))) {
       return S_OK;  // don't care about failures.
    }

    ULONG fetched_count = 0;
    base::win::ScopedComPtr<ITfRange> range;
    if (FAILED(ranges->Next(1, range.Receive(), &fetched_count)))
      return S_OK;  // don't care about failures.

    // |fetched_count| != 0 means there is at least one range that contains
    // updated texts.
    if (fetched_count != 0)
      observer_->OnTextUpdated();
    return S_OK;
  }

  // ITfUiElementSink override.
  virtual STDMETHODIMP BeginUIElement(DWORD element_id, BOOL* is_show) {
    if (is_show)
      *is_show = TRUE;  // without this the UI element will not be shown.

    if (!IsCandidateWindowInternal(element_id))
      return S_OK;

    std::pair<std::set<DWORD>::iterator, bool> insert_result =
        open_candidate_window_ids_.insert(element_id);

    if (!observer_)
      return S_OK;

    // Don't call if |element_id| is already handled.
    if (!insert_result.second)
      return S_OK;

    observer_->OnCandidateWindowCountChanged(open_candidate_window_ids_.size());

    return S_OK;
  }

  // ITfUiElementSink override.
  virtual STDMETHODIMP UpdateUIElement(DWORD element_id) {
    return S_OK;
  }

  // ITfUiElementSink override.
  virtual STDMETHODIMP EndUIElement(DWORD element_id) {
    if (open_candidate_window_ids_.erase(element_id) == 0)
      return S_OK;

    if (!observer_)
      return S_OK;

    observer_->OnCandidateWindowCountChanged(open_candidate_window_ids_.size());

    return S_OK;
  }

  // TsfEventRouter override.
  virtual void SetManager(ITfThreadMgr* manager, Observer* observer) OVERRIDE {
    EnsureDeassociated();
    if (manager && observer) {
      Associate(manager, observer);
    }
  }

  // TsfEventRouter override.
  virtual bool IsImeComposing() OVERRIDE {
    DCHECK(base::win::IsTsfAwareRequired())
        << "Do not call without TSF environment.";
    if (!context_)
      return false;
    return IsImeComposingInternal(context_);
  }

 private:
  // Returns true if the given |context| is in composing.
  static bool IsImeComposingInternal(ITfContext* context) {
    DCHECK(base::win::IsTsfAwareRequired())
        << "Do not call without TSF environment.";
    DCHECK(context);
    base::win::ScopedComPtr<ITfContextComposition> context_composition;
    if (FAILED(context_composition.QueryFrom(context)))
      return false;
    base::win::ScopedComPtr<IEnumITfCompositionView> enum_composition_view;
    if (FAILED(context_composition->EnumCompositions(
            enum_composition_view.Receive())))
      return false;
    base::win::ScopedComPtr<ITfCompositionView> composition_view;
    return enum_composition_view->Next(1, composition_view.Receive(),
               NULL) == S_OK;
  }

  // Returns true if the given |element_id| represents candidate window.
  bool IsCandidateWindowInternal(DWORD element_id) {
    DCHECK(ui_element_manager_.get());
    base::win::ScopedComPtr<ITfUIElement> ui_element;
    if (FAILED(ui_element_manager_->GetUIElement(element_id,
                                                 ui_element.Receive()))) {
      return false;
    }
    base::win::ScopedComPtr<ITfCandidateListUIElement>
        candidate_list_ui_element;
    return SUCCEEDED(candidate_list_ui_element.QueryFrom(ui_element));
  }

  // Associates this class with specified |manager|.
  void Associate(ITfThreadMgr* thread_manager, Observer* observer) {
    DCHECK(base::win::IsTsfAwareRequired())
        << "Do not call without TSF environment.";
    DCHECK(thread_manager);

    base::win::ScopedComPtr<ITfDocumentMgr> document_manager;
    if (FAILED(thread_manager->GetFocus(document_manager.Receive())))
      return;

    if (FAILED(document_manager->GetBase(context_.Receive())))
      return;
    if (FAILED(context_source_.QueryFrom(context_)))
      return;
    context_source_->AdviseSink(IID_ITfTextEditSink,
                                static_cast<ITfTextEditSink*>(this),
                                &context_source_cookie_);

    if (FAILED(ui_element_manager_.QueryFrom(thread_manager)))
      return;
    if (FAILED(ui_source_.QueryFrom(ui_element_manager_)))
      return;
    ui_source_->AdviseSink(IID_ITfUIElementSink,
                           static_cast<ITfUIElementSink*>(this),
                           &ui_source_cookie_);
    observer_ = observer;
  }

  // Resets the association, this function is safe to call if there is no
  // associated thread manager.
  void EnsureDeassociated() {
    DCHECK(base::win::IsTsfAwareRequired())
        << "Do not call without TSF environment.";

    context_.Release();

    if (context_source_) {
      context_source_->UnadviseSink(context_source_cookie_);
      context_source_.Release();
    }
    context_source_cookie_ = TF_INVALID_COOKIE;

    ui_element_manager_.Release();
    if (ui_source_) {
      ui_source_->UnadviseSink(ui_source_cookie_);
      ui_source_.Release();
    }
    ui_source_cookie_ = TF_INVALID_COOKIE;

    observer_ = NULL;
  }

  Observer* observer_;

  // A context associated with this class.
  base::win::ScopedComPtr<ITfContext> context_;

  // The ITfSource associated with |context_|.
  base::win::ScopedComPtr<ITfSource> context_source_;

  // The cookie for |context_source_|.
  DWORD context_source_cookie_;

  // A UiElementMgr associated with this class.
  base::win::ScopedComPtr<ITfUIElementMgr> ui_element_manager_;

  // The ITfSouce associated with |ui_element_manager_|.
  base::win::ScopedComPtr<ITfSource> ui_source_;

  // The cookie for |ui_source_|.
  DWORD ui_source_cookie_;

  // The set of currently opend candidate window ids.
  std::set<DWORD> open_candidate_window_ids_;

  // The reference count of this instance.
  volatile LONG ref_count_;

  DISALLOW_COPY_AND_ASSIGN(TsfEventRouterImpl);
};

///////////////////////////////////////////////////////////////////////////////
// TsfEventRouter

TsfEventRouter::TsfEventRouter() {
}

TsfEventRouter::~TsfEventRouter() {
}

TsfEventRouter* TsfEventRouter::Create() {
  return new TsfEventRouterImpl();
}

}  // namespace ui
