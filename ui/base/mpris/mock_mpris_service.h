// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MPRIS_MOCK_MPRIS_SERVICE_H_
#define UI_BASE_MPRIS_MOCK_MPRIS_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/mpris/mpris_service.h"

namespace mpris {

class MprisServiceObserver;

// Mock implementation of MprisService for testing.
class MockMprisService : public MprisService {
 public:
  MockMprisService();
  ~MockMprisService() override;

  // MprisService implementation.
  MOCK_METHOD0(StartService, void());
  MOCK_METHOD1(AddObserver, void(MprisServiceObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(MprisServiceObserver* observer));
  MOCK_METHOD1(SetCanGoNext, void(bool value));
  MOCK_METHOD1(SetCanGoPrevious, void(bool value));
  MOCK_METHOD1(SetCanPlay, void(bool value));
  MOCK_METHOD1(SetCanPause, void(bool value));
  MOCK_CONST_METHOD0(GetServiceName, std::string());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMprisService);
};

}  // namespace mpris

#endif  // UI_BASE_MPRIS_MOCK_MPRIS_SERVICE_H_
