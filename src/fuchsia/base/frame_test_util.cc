// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/frame_test_util.h"

#include "base/run_loop.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"

namespace cr_fuchsia {

bool LoadUrlAndExpectResponse(
    fuchsia::web::NavigationController* navigation_controller,
    fuchsia::web::LoadUrlParams load_url_params,
    std::string url) {
  DCHECK(navigation_controller);
  base::RunLoop run_loop;
  ResultReceiver<fuchsia::web::NavigationController_LoadUrl_Result> result(
      run_loop.QuitClosure());
  navigation_controller->LoadUrl(
      url, std::move(load_url_params),
      CallbackToFitFunction(result.GetReceiveCallback()));
  run_loop.Run();
  return result->is_response();
}

}  // namespace cr_fuchsia
