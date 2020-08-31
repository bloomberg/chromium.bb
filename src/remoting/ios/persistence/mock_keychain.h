// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_PERSISTENCE_MOCK_KEYCHAIN_H_
#define REMOTING_IOS_PERSISTENCE_MOCK_KEYCHAIN_H_

#include "remoting/ios/persistence/keychain.h"

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockKeychain : public Keychain {
 public:
  MockKeychain();
  ~MockKeychain() override;

  void ExpectAndCaptureSetData(Key key,
                               const std::string& account,
                               std::string* out_data);
  void ExpectGetDataAndReturn(Key key,
                              const std::string& account,
                              const std::string& data_to_return);

  // Mocks.
  MOCK_METHOD3(SetData, void(Key, const std::string&, const std::string&));
  MOCK_METHOD2(RemoveData, void(Key, const std::string&));
  MOCK_CONST_METHOD2(GetData, std::string(Key, const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeychain);
};

}  // namespace remoting

#endif  // REMOTING_IOS_PERSISTENCE_MOCK_KEYCHAIN_H_
