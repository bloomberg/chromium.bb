// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_QUOTA_PERMISSION_CONTEXT_H_
#define CHROMECAST_BROWSER_CAST_QUOTA_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "content/public/browser/quota_permission_context.h"

namespace chromecast {

class CastQuotaPermissionContext : public content::QuotaPermissionContext {
 public:
  CastQuotaPermissionContext();

  // content::QuotaPermissionContext implementation:
  void RequestQuotaPermission(const content::StorageQuotaParams& params,
                              int render_process_id,
                              PermissionCallback callback) override;

 private:
  ~CastQuotaPermissionContext() override;

  DISALLOW_COPY_AND_ASSIGN(CastQuotaPermissionContext);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_QUOTA_PERMISSION_CONTEXT_H_
