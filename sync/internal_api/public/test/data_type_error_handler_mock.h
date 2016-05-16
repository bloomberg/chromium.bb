// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_DATA_TYPE_ERROR_HANDLER_MOCK_H__
#define SYNC_INTERNAL_API_PUBLIC_TEST_DATA_TYPE_ERROR_HANDLER_MOCK_H__

#include <string>

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/data_type_error_handler.h"

namespace syncer {

// A mock DataTypeErrorHandler for testing. Set the expected error type with
// ExpectError and OnSingleDataTypeUnrecoverableError will pass. If the error is
// not called this object's destructor will DCHECK.
class DataTypeErrorHandlerMock : public DataTypeErrorHandler {
 public:
  DataTypeErrorHandlerMock();
  ~DataTypeErrorHandlerMock() override;

  void OnSingleDataTypeUnrecoverableError(const SyncError& error) override;
  SyncError CreateAndUploadError(const tracked_objects::Location& location,
                                 const std::string& message,
                                 ModelType type) override;

  // Set the |error_type| to expect.
  void ExpectError(SyncError::ErrorType error_type);

 private:
  // The error type to expect.
  SyncError::ErrorType expected_error_type_ = SyncError::UNSET;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_DATA_TYPE_ERROR_HANDLER_MOCK_H__
