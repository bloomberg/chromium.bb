// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_COMMON_CONTENT_CLIENT_IMPL_H_
#define WEBLAYER_COMMON_CONTENT_CLIENT_IMPL_H_

#include "base/synchronization/lock.h"
#include "content/public/common/content_client.h"

namespace embedder_support {
class OriginTrialPolicyImpl;
}

namespace weblayer {

class ContentClientImpl : public content::ContentClient {
 public:
  ContentClientImpl();
  ~ContentClientImpl() override;

  base::string16 GetLocalizedString(int message_id) override;
  base::string16 GetLocalizedString(int message_id,
                                    const base::string16& replacement) override;
  base::StringPiece GetDataResource(int resource_id,
                                    ui::ScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  void SetGpuInfo(const gpu::GPUInfo& gpu_info) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;

 private:
  // Used to lock when |origin_trial_policy_| is initialized.
  base::Lock origin_trial_policy_lock_;
  std::unique_ptr<embedder_support::OriginTrialPolicyImpl> origin_trial_policy_;
};

}  // namespace weblayer

#endif  // WEBLAYER_COMMON_CONTENT_CLIENT_IMPL_H_
