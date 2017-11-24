// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAssociatedURLLoaderImpl_h
#define WebAssociatedURLLoaderImpl_h

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "public/web/WebAssociatedURLLoader.h"
#include "public/web/WebAssociatedURLLoaderOptions.h"

namespace blink {

class DocumentThreadableLoader;
class WebAssociatedURLLoaderClient;
class Document;

// This class is used to implement WebFrame::createAssociatedURLLoader.
class CORE_EXPORT WebAssociatedURLLoaderImpl final
    : public WebAssociatedURLLoader {
 public:
  WebAssociatedURLLoaderImpl(Document*, const WebAssociatedURLLoaderOptions&);
  ~WebAssociatedURLLoaderImpl();

  void LoadAsynchronously(const WebURLRequest&,
                          WebAssociatedURLLoaderClient*) override;
  void Cancel() override;
  void SetDefersLoading(bool) override;
  void SetLoadingTaskRunner(blink::WebTaskRunner*) override;

  // Called by |m_observer| to handle destruction of the Document associated
  // with the frame given to the constructor.
  void DocumentDestroyed();

  // Called by ClientAdapter to handle completion of loading.
  void ClientAdapterDone();

 private:
  class ClientAdapter;
  class Observer;

  void CancelLoader();
  void DisposeObserver();

  WebAssociatedURLLoaderClient* ReleaseClient() {
    WebAssociatedURLLoaderClient* client = client_;
    client_ = nullptr;
    return client;
  }

  WebAssociatedURLLoaderClient* client_;
  WebAssociatedURLLoaderOptions options_;

  // An adapter which converts the DocumentThreadableLoaderClient method
  // calls into the WebURLLoaderClient method calls.
  std::unique_ptr<ClientAdapter> client_adapter_;
  Persistent<DocumentThreadableLoader> loader_;

  // A ContextLifecycleObserver for cancelling |m_loader| when the Document
  // is detached.
  Persistent<Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(WebAssociatedURLLoaderImpl);
};

}  // namespace blink

#endif
