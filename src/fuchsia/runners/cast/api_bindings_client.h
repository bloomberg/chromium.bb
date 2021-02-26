// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_API_BINDINGS_CLIENT_H_
#define FUCHSIA_RUNNERS_CAST_API_BINDINGS_CLIENT_H_

#include <fuchsia/web/cpp/fidl.h>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast/named_message_port_connector/named_message_port_connector.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

// Injects scripts received from the ApiBindings service, and provides connected
// ports to the Agent.
class ApiBindingsClient {
 public:
  // Reads bindings definitions from |bindings_service_| at construction time.
  // |on_initialization_complete| is invoked when either the initial bindings
  // have been received, or on failure. The caller should use HasBindings()
  // to verify that bindings were received, and may then use AttachToFrame().
  ApiBindingsClient(
      fidl::InterfaceHandle<chromium::cast::ApiBindings> bindings_service,
      base::OnceClosure on_initialization_complete);
  ~ApiBindingsClient();

  // Injects APIs and handles channel connections on |frame|.
  // |on_error_callback| is invoked in the event of an unrecoverable error (e.g.
  // lost connection to the Agent). The callback must remain valid for the
  // entire lifetime of |this|.
  void AttachToFrame(fuchsia::web::Frame* frame,
                     cast_api_bindings::NamedMessagePortConnector* connector,
                     base::OnceClosure on_error_callback);

  // Indicates that the Frame is no longer live, preventing the API bindings
  // client from attempting to remove injected bindings from it.
  void DetachFromFrame(fuchsia::web::Frame* frame);

  // Indicates that bindings were successfully received from
  // |bindings_service_|.
  bool HasBindings() const;

  // TODO(crbug.com/1082821): Move this method back to private once the Cast
  // Streaming Receiver component has been implemented.
  // Called when |connector_| has connected a port.
  bool OnPortConnected(base::StringPiece port_name,
                       std::unique_ptr<cast_api_bindings::MessagePort> port);

 private:
  // Called when ApiBindings::GetAll() has responded.
  void OnBindingsReceived(std::vector<chromium::cast::ApiBinding> bindings);

  base::Optional<std::vector<chromium::cast::ApiBinding>> bindings_;
  fuchsia::web::Frame* frame_ = nullptr;
  cast_api_bindings::NamedMessagePortConnector* connector_ = nullptr;
  chromium::cast::ApiBindingsPtr bindings_service_;
  base::OnceClosure on_initialization_complete_;

  DISALLOW_COPY_AND_ASSIGN(ApiBindingsClient);
};

#endif  // FUCHSIA_RUNNERS_CAST_API_BINDINGS_CLIENT_H_
