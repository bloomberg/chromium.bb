// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_TEST_MOCK_MULTIROOM_MANAGER_H_
#define CHROMECAST_MEDIA_CMA_TEST_MOCK_MULTIROOM_MANAGER_H_

#include <string>
#include <utility>

#include "base/bind.h"
#include "chromecast/common/mojom/multiroom.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace chromecast {
namespace media {
class MockMultiroomManager : public mojom::MultiroomManager {
 public:
  MockMultiroomManager();
  ~MockMultiroomManager() override;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    binding_.Close();
    binding_.Bind(mojom::MultiroomManagerRequest(std::move(handle)));
  }

  void GetMultiroomInfo(const std::string& session_id,
                        GetMultiroomInfoCallback callback) override;

  void SetMultiroomInfo(chromecast::mojom::MultiroomInfo info);
  const std::string GetLastSessionId() { return last_session_id_; }

 private:
  mojo::Binding<mojom::MultiroomManager> binding_;
  chromecast::mojom::MultiroomInfo info_;
  std::string last_session_id_;
};

inline MockMultiroomManager::MockMultiroomManager()
    : binding_(this), last_session_id_("default_session_id") {}

inline MockMultiroomManager::~MockMultiroomManager() = default;

inline void MockMultiroomManager::GetMultiroomInfo(
    const std::string& session_id,
    GetMultiroomInfoCallback callback) {
  last_session_id_ = session_id;
  std::move(callback).Run(info_.Clone());
}

inline void MockMultiroomManager::SetMultiroomInfo(
    chromecast::mojom::MultiroomInfo info) {
  info_ = std::move(info);
}

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_TEST_MOCK_MULTIROOM_MANAGER_H_
