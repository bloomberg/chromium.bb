// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_DATA_TYPE_ERROR_HANDLER_MOCK_H__
#define SYNC_INTERNAL_API_PUBLIC_TEST_DATA_TYPE_ERROR_HANDLER_MOCK_H__

#include <string>

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/data_type_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class DataTypeErrorHandlerMock : public DataTypeErrorHandler {
 public:
  DataTypeErrorHandlerMock();
  virtual ~DataTypeErrorHandlerMock();
  MOCK_METHOD1(OnSingleDataTypeUnrecoverableError, void(const SyncError&));
  MOCK_METHOD3(CreateAndUploadError,
               SyncError(const tracked_objects::Location&,
                         const std::string&,
                         ModelType));
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_DATA_TYPE_ERROR_HANDLER_MOCK_H__
