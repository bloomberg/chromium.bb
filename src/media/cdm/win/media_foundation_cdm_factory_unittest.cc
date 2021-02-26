// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm_factory.h"

#include "base/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

const char kClearKeyKeySystem[] = "org.w3.clearkey";
const CdmConfig kHardwareSecureCdmConfig = {true, true, true};

using Microsoft::WRL::ComPtr;

class MediaFoundationCdmFactoryTest : public testing::Test {
 public:
  MediaFoundationCdmFactoryTest()
      : mf_cdm_factory_(MakeComPtr<MockMFCdmFactory>()),
        mf_cdm_access_(MakeComPtr<MockMFCdmAccess>()),
        mf_cdm_(MakeComPtr<MockMFCdm>()) {}

  ~MediaFoundationCdmFactoryTest() override = default;

  HRESULT GetMockCdmFactory(
      bool expect_success,
      ComPtr<IMFContentDecryptionModuleFactory>& mf_cdm_factory) {
    if (!expect_success)
      return E_FAIL;

    mf_cdm_factory = mf_cdm_factory_;
    return S_OK;
  }

  void SetCreateCdmFactoryCallback(bool expect_success) {
    cdm_factory_.SetCreateCdmFactoryCallback(
        kClearKeyKeySystem,
        base::BindRepeating(&MediaFoundationCdmFactoryTest::GetMockCdmFactory,
                            base::Unretained(this), expect_success));
  }

 protected:
  void Create() {
    cdm_factory_.Create(
        kClearKeyKeySystem, kHardwareSecureCdmConfig,
        base::BindRepeating(&MockCdmClient::OnSessionMessage,
                            base::Unretained(&cdm_client_)),
        base::BindRepeating(&MockCdmClient::OnSessionClosed,
                            base::Unretained(&cdm_client_)),
        base::BindRepeating(&MockCdmClient::OnSessionKeysChange,
                            base::Unretained(&cdm_client_)),
        base::BindRepeating(&MockCdmClient::OnSessionExpirationUpdate,
                            base::Unretained(&cdm_client_)),
        cdm_created_cb_.Get());
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;

  StrictMock<MockCdmClient> cdm_client_;
  ComPtr<MockMFCdmFactory> mf_cdm_factory_;
  ComPtr<MockMFCdmAccess> mf_cdm_access_;
  ComPtr<MockMFCdm> mf_cdm_;
  MediaFoundationCdmFactory cdm_factory_;
  base::MockCallback<CdmCreatedCB> cdm_created_cb_;
};

TEST_F(MediaFoundationCdmFactoryTest, Create) {
  SetCreateCdmFactoryCallback(/*expect_success=*/true);

  COM_EXPECT_CALL(mf_cdm_factory_, IsTypeSupported(NotNull(), IsNull()))
      .WillOnce(Return(TRUE));
  COM_EXPECT_CALL(mf_cdm_factory_, CreateContentDecryptionModuleAccess(
                                       NotNull(), NotNull(), _, _))
      .WillOnce(DoAll(SetComPointee<3>(mf_cdm_access_.Get()), Return(S_OK)));
  COM_EXPECT_CALL(mf_cdm_access_, CreateContentDecryptionModule(NotNull(), _))
      .WillOnce(DoAll(SetComPointee<1>(mf_cdm_.Get()), Return(S_OK)));

  EXPECT_CALL(cdm_created_cb_, Run(NotNull(), _));
  Create();
}

TEST_F(MediaFoundationCdmFactoryTest, CreateCdmFactoryFail) {
  SetCreateCdmFactoryCallback(/*expect_success=*/false);

  EXPECT_CALL(cdm_created_cb_, Run(IsNull(), _));
  Create();
}

TEST_F(MediaFoundationCdmFactoryTest, IsTypeSupportedFail) {
  SetCreateCdmFactoryCallback(/*expect_success=*/true);

  COM_EXPECT_CALL(mf_cdm_factory_, IsTypeSupported(NotNull(), IsNull()))
      .WillOnce(Return(FALSE));

  EXPECT_CALL(cdm_created_cb_, Run(IsNull(), _));
  Create();
}

TEST_F(MediaFoundationCdmFactoryTest, CreateCdmAccessFail) {
  SetCreateCdmFactoryCallback(/*expect_success=*/true);

  COM_EXPECT_CALL(mf_cdm_factory_, IsTypeSupported(NotNull(), IsNull()))
      .WillOnce(Return(TRUE));
  COM_EXPECT_CALL(mf_cdm_factory_, CreateContentDecryptionModuleAccess(
                                       NotNull(), NotNull(), _, _))
      .WillOnce(Return(E_FAIL));

  EXPECT_CALL(cdm_created_cb_, Run(IsNull(), _));
  Create();
}

TEST_F(MediaFoundationCdmFactoryTest, CreateCdmFail) {
  SetCreateCdmFactoryCallback(/*expect_success=*/true);

  COM_EXPECT_CALL(mf_cdm_factory_, IsTypeSupported(NotNull(), IsNull()))
      .WillOnce(Return(TRUE));
  COM_EXPECT_CALL(mf_cdm_factory_, CreateContentDecryptionModuleAccess(
                                       NotNull(), NotNull(), _, _))
      .WillOnce(DoAll(SetComPointee<3>(mf_cdm_access_.Get()), Return(S_OK)));
  COM_EXPECT_CALL(mf_cdm_access_, CreateContentDecryptionModule(NotNull(), _))
      .WillOnce(DoAll(SetComPointee<1>(mf_cdm_.Get()), Return(E_FAIL)));

  EXPECT_CALL(cdm_created_cb_, Run(IsNull(), _));
  Create();
}

}  // namespace media
