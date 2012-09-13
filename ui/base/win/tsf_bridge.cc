// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <msctf.h>
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/threading/thread_local_storage.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/win/tsf_bridge.h"
#include "ui/base/win/tsf_text_store.h"

namespace ui {

namespace {

// A TLS implementation of TsfBridge.
class TsfBridgeDelegate : public TsfBridge {
 public:
  TsfBridgeDelegate()
      : source_cookie_(TF_INVALID_COOKIE),
        client_id_(TF_CLIENTID_NULL),
        client_(NULL) {
  }

  virtual ~TsfBridgeDelegate() {
  }

  // TsfBridge override.
  bool Initialize() {
    DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
    if (client_id_ != TF_CLIENTID_NULL) {
      VLOG(1) << "Already initialized.";
      return false;
    }

    text_store_.reset(new scoped_refptr<TsfTextStore>(new TsfTextStore()));

    if (FAILED(thread_manager_.CreateInstance(CLSID_TF_ThreadMgr))) {
      VLOG(1) << "Failed to create ThreadManager instance.";
      return false;
    }

    if (!InitializeForEnabledDocumentManager())
      return false;

    if (!InitializeForDisabledDocumentManager())
      return false;

    // Japanese IME expectes the default value of this compartment is
    // TF_SENTENCEMODE_PHRASEPREDICT like IMM32 implementation. This value is
    // managed per thread, so that it is enough to set this value at once. This
    // value does not affect other language's IME behaviors.
    base::win::ScopedComPtr<ITfCompartmentMgr> thread_compartment_manager;
    if (FAILED(thread_compartment_manager.QueryFrom(thread_manager_))) {
      VLOG(1) << "Failed to get ITfCompartmentMgr.";
      return false;
    }

    base::win::ScopedComPtr<ITfCompartment> sentence_compartment;
    if (FAILED(thread_compartment_manager->GetCompartment(
        GUID_COMPARTMENT_KEYBOARD_INPUTMODE_SENTENCE,
        sentence_compartment.Receive()))) {
      VLOG(1) << "Failed to get sentence compartment.";
      return false;
    }

    base::win::ScopedVariant sentence_variant;
    sentence_variant.Set(TF_SENTENCEMODE_PHRASEPREDICT);
    if (FAILED(sentence_compartment->SetValue(client_id_, &sentence_variant))) {
      VLOG(1) << "Failed to change the sentence mode.";
      return false;
    }

    return true;
  }

  // TsfBridge override.
  virtual void Shutdown() OVERRIDE {
    DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
    if (!IsInitialized())
      return;
    base::win::ScopedComPtr<ITfContext> context;
    if (FAILED(document_manager_->GetTop(context.Receive())))
      return;
    base::win::ScopedComPtr<ITfSource> source;
    if (FAILED(source.QueryFrom(context)))
      return;
    if (source_cookie_ == TF_INVALID_COOKIE)
      return;
    if (FAILED(source->UnadviseSink(source_cookie_)))
      return;

    DCHECK(text_store_.get());
    text_store_.reset();
    client_id_ = TF_CLIENTID_NULL;
  }

  // TsfBridge override.
  virtual void OnTextInputTypeChanged(TextInputClient* client) OVERRIDE {
    DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
    DCHECK(IsInitialized());

    if (client != client_) {
      // Called from not focusing client. Do nothing.
      return;
    }

    DCHECK(client_);
    const TextInputType type = client_->GetTextInputType();
    const bool is_ime_enabled =
        type != TEXT_INPUT_TYPE_NONE && type != TEXT_INPUT_TYPE_PASSWORD;
    thread_manager_->SetFocus(
        is_ime_enabled ? document_manager_ : disabled_document_manager_);
  }

  // TsfBridge override.
  virtual bool CancelComposition() OVERRIDE {
    DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
    DCHECK(IsInitialized());
    // If the current focused document manager is not |document_manager_|, do
    // nothing here.
    if (!IsFocused(document_manager_))
      return false;

    base::win::ScopedComPtr<ITfContext> context;
    if (FAILED(document_manager_->GetTop(context.Receive()))) {
      VLOG(1) << "Failed to get top context.";
      return false;
    }

    base::win::ScopedComPtr<ITfContextOwnerCompositionServices> owner;
    if (FAILED(owner.QueryFrom(context))) {
      VLOG(1) << "Failed to get ITfContextOwnerCompositionService.";
      return false;
    }
    // Cancel all composition.
    owner->TerminateComposition(NULL);
    return true;
  }

  // TsfBridge override.
  virtual void SetFocusedClient(HWND focused_window,
                                TextInputClient* client) OVERRIDE {
    DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
    DCHECK(client);
    DCHECK(IsInitialized());
    client_ = client;
    text_store_->get()->SetFocusedTextInputClient(focused_window, client);

    // Synchronize text input type state.
    OnTextInputTypeChanged(client);
  }

  // TsfBridge override.
  virtual void RemoveFocusedClient(TextInputClient* client) OVERRIDE {
    DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
    DCHECK(IsInitialized());
    if (client_ == client) {
      client_ = NULL;
      text_store_->get()->SetFocusedTextInputClient(NULL, NULL);
    }
  }

 private:
  friend struct DefaultSingletonTraits<TsfBridgeDelegate>;

  // Initializes |document_manager_|.
  bool InitializeForEnabledDocumentManager() {
    if (FAILED(thread_manager_->Activate(&client_id_))) {
      VLOG(1) << "Failed to activate Thread Manager.";
      return false;
    }

    if (FAILED(thread_manager_->CreateDocumentMgr(
            document_manager_.Receive()))) {
      VLOG(1) << "Failed to create Document Manager.";
      return false;
    }

    TfEditCookie unused_edit_cookie = TF_INVALID_EDIT_COOKIE;
    base::win::ScopedComPtr<ITfContext> context;
    if (FAILED(document_manager_->CreateContext(client_id_,
                                                0,
                                                static_cast<ITextStoreACP*>(
                                                    text_store_->get()),
                                                context.Receive(),
                                                &unused_edit_cookie))) {
      VLOG(1) << "Failed to create Context.";
      return false;
    }

    if (FAILED(document_manager_->Push(context))) {
      VLOG(1) << "Failed to push context.";
      return false;
    }

    if (FAILED(thread_manager_->CreateDocumentMgr(
        disabled_document_manager_.Receive()))) {
      VLOG(1) << "Failed to create Document Manager.";
      return false;
    }

    base::win::ScopedComPtr<ITfSource> source;
    if (FAILED(source.QueryFrom(context))) {
      VLOG(1) << "Failed to get source.";
      return false;
    }

    if (FAILED(source->AdviseSink(IID_ITfTextEditSink,
                                  static_cast<ITfTextEditSink*>(
                                      text_store_->get()),
                                  &source_cookie_))) {
      VLOG(1) << "AdviseSink failed.";
      return false;
    }

    if (source_cookie_ == TF_INVALID_COOKIE) {
      VLOG(1) << "The result of cookie is invalid.";
      return false;
    }

    return true;
  }

  // Initializes |disabled_document_manager_|.
  bool InitializeForDisabledDocumentManager() {
    base::win::ScopedComPtr<ITfContext> disabled_context;
    DWORD disabled_edit_cookie;
    if (document_manager_->CreateContext(client_id_,
                                         0,
                                         NULL,
                                         disabled_context.Receive(),
                                         &disabled_edit_cookie)) {
      VLOG(1) << "Failed to create disabled Context";
      return false;
    }

    base::win::ScopedComPtr<ITfCompartmentMgr> compartment_mgr;
    if (FAILED(compartment_mgr.QueryFrom(disabled_context))) {
      VLOG(1) << "Failed to get CompartmentMgr.";
      return false;
    }

    base::win::ScopedComPtr<ITfCompartment> disabled_compartment;
    if (FAILED(compartment_mgr->GetCompartment(
        GUID_COMPARTMENT_KEYBOARD_DISABLED,
        disabled_compartment.Receive()))) {
      VLOG(1) << "Failed to get keyboard disabled compartment.";
      return false;
    }

    base::win::ScopedVariant variant;
    variant.Set(static_cast<int32>(1));
    if (FAILED(disabled_compartment->SetValue(client_id_, &variant))) {
      VLOG(1) << "Failed to disable the DocumentMgr.";
      return false;
    }

    base::win::ScopedComPtr<ITfCompartment> empty_context;
    if (FAILED(compartment_mgr->GetCompartment(GUID_COMPARTMENT_EMPTYCONTEXT,
                                               empty_context.Receive()))) {
      VLOG(1) << "Failed to get empty context compartment.";
      return false;
    }
    base::win::ScopedVariant empty_context_variant;
    empty_context_variant.Set(static_cast<int32>(1));
    if (FAILED(empty_context->SetValue(client_id_, &empty_context_variant))) {
      VLOG(1) << "Failed to set empty context.";
      return false;
    }

    if (FAILED(disabled_document_manager_->Push(disabled_context))) {
      VLOG(1) << "Failed to push disabled context.";
      return false;
    }

    return true;
  }

  // Returns true if |document_manager| is the focused document manager.
  bool IsFocused(base::win::ScopedComPtr<ITfDocumentMgr> document_manager) {
    base::win::ScopedComPtr<ITfDocumentMgr> focused_document_mangaer;
    if (FAILED(thread_manager_->GetFocus(focused_document_mangaer.Receive())))
      return false;
    return focused_document_mangaer.IsSameObject(document_manager);
  }

  // Returns true if already initialized.
  bool IsInitialized() {
    return client_id_ != TF_CLIENTID_NULL;
  }

  // An ITfContext object to be used in normal text field.
  base::win::ScopedComPtr<ITfDocumentMgr> document_manager_;

  // Altough TSF support IME enable/disable switching without context changing,
  // some IMEs don't change their state. So we should switch a focus to a
  // |disabled_document_manager_| which contains deactivated context for
  // disabling IMEs.
  base::win::ScopedComPtr<ITfDocumentMgr> disabled_document_manager_;

  // An ITfThreadMgr object to be used in focus and document management.
  base::win::ScopedComPtr<ITfThreadMgr> thread_manager_;

  // A TextStore object to be used in communicating with IME.
  scoped_ptr<scoped_refptr<TsfTextStore> > text_store_;

  DWORD source_cookie_;
  TfClientId client_id_;

  // Current focused text input client. Do not free |client_|.
  TextInputClient* client_;

  DISALLOW_COPY_AND_ASSIGN(TsfBridgeDelegate);
};

// We use thread local storage for TsfBridge lifespan management.
base::ThreadLocalStorage::StaticSlot tls_tsf_bridge = TLS_INITIALIZER;

// Used for releasing TLS object.
void FreeTlsTsfBridgeDelegate(void* data) {
  TsfBridgeDelegate* delegate = static_cast<TsfBridgeDelegate*>(data);
  delete delegate;
}

}  // namespace

TsfBridge::TsfBridge() {
}

TsfBridge::~TsfBridge() {
}

// static
bool TsfBridge::Initialize() {
  if (MessageLoop::current()->type() != MessageLoop::TYPE_UI) {
    VLOG(1) << "Do not use TsfBridge without UI thread.";
    return false;
  }
  tls_tsf_bridge.Initialize(FreeTlsTsfBridgeDelegate);
  TsfBridgeDelegate* delegate = new TsfBridgeDelegate();
  tls_tsf_bridge.Set(delegate);
  return delegate->Initialize();
}

// static
TsfBridge* TsfBridge::GetInstance() {
  if (MessageLoop::current()->type() != MessageLoop::TYPE_UI) {
    VLOG(1) << "Do not use TsfBridge without UI thread.";
    return NULL;
  }
  TsfBridgeDelegate* delegate =
      static_cast<TsfBridgeDelegate*>(tls_tsf_bridge.Get());
  DCHECK(delegate) << "Do no call GetInstance before TsfBridge::Initialize.";
  return delegate;
}

}  // namespace ui
