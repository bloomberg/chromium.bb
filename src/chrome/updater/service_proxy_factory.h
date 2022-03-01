// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVICE_PROXY_FACTORY_H_
#define CHROME_UPDATER_SERVICE_PROXY_FACTORY_H_

#include "base/memory/scoped_refptr.h"

namespace updater {

enum class UpdaterScope;
class UpdateService;
class UpdateServiceInternal;

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope updater_scope);

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope updater_scope);

}  // namespace updater

#endif  // CHROME_UPDATER_SERVICE_PROXY_FACTORY_H_
