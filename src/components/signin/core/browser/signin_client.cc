// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_client.h"

void SigninClient::PreSignOut(
    const base::Callback<void()>& sign_out,
    signin_metrics::ProfileSignout signout_source_metric) {
  sign_out.Run();
}

void SigninClient::PreGaiaLogout(base::OnceClosure callback) {
  std::move(callback).Run();
}
