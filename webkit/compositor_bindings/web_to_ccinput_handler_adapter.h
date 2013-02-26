// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebToCCInputHandlerAdapter_h
#define WebToCCInputHandlerAdapter_h

#include "base/memory/scoped_ptr.h"
#include "cc/input_handler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebInputHandler.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit {

class WebToCCInputHandlerAdapter : public cc::InputHandler {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT static 
      scoped_ptr<WebToCCInputHandlerAdapter> create(
          scoped_ptr<WebInputHandler>);
  virtual ~WebToCCInputHandlerAdapter();

  // cc::InputHandler implementation.
  virtual void bindToClient(cc::InputHandlerClient*) OVERRIDE;
  virtual void animate(base::TimeTicks time) OVERRIDE;
  virtual void mainThreadHasStoppedFlinging() OVERRIDE;

 private:
  explicit WebToCCInputHandlerAdapter(scoped_ptr<WebInputHandler>);

  class ClientAdapter;
  scoped_ptr<ClientAdapter> client_adapter_;
  scoped_ptr<WebInputHandler> handler_;
};

}

#endif  // WebToCCInputHandlerAdapter_h
