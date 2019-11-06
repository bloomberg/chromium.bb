// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_QUERY_PARAMS_DELEGATE_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_QUERY_PARAMS_DELEGATE_H_

#include <string>

#include "base/macros.h"

namespace update_client {

// Embedders can specify an UpdateQueryParamsDelegate to provide additional
// custom parameters. If not specified (Set is never called), no additional
// parameters are added.
class UpdateQueryParamsDelegate {
 public:
  UpdateQueryParamsDelegate();
  virtual ~UpdateQueryParamsDelegate();

  // Returns additional parameters, if any. If there are any parameters, the
  // string should begin with a & character.
  virtual std::string GetExtraParams() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateQueryParamsDelegate);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_QUERY_PARAMS_DELEGATE_H_
