// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_TEST_FRAME_TEST_UTIL_H_
#define FUCHSIA_BASE_TEST_FRAME_TEST_UTIL_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cr_fuchsia {

// Uses |navigation_controller| to load |url| with |load_url_params|. Returns
// after the load is completed. Returns true if the load was successful, false
// otherwise.
bool LoadUrlAndExpectResponse(
    fuchsia::web::NavigationController* navigation_controller,
    fuchsia::web::LoadUrlParams load_url_params,
    base::StringPiece url);

// Executes |script| in the context of |frame|'s top-level document.
// Returns an un-set |absl::optional<>| on failure.
absl::optional<base::Value> ExecuteJavaScript(fuchsia::web::Frame* frame,
                                              base::StringPiece script);

// Creates and returns a LoadUrlParams with was_user_activated set to true.
// This allows user actions to propagate to the frame, allowing features such as
// autoplay to be used, which is used by many media tests.
fuchsia::web::LoadUrlParams CreateLoadUrlParamsWithUserActivation();

// Creates a WebMessage with one outgoing transferable set to
// |message_port_request| and data set to |buffer|.
fuchsia::web::WebMessage CreateWebMessageWithMessagePortRequest(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
    fuchsia::mem::Buffer buffer);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_TEST_FRAME_TEST_UTIL_H_
