// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_WORK_ITEM_MOCKS_H_
#define CHROME_INSTALLER_UTIL_WORK_ITEM_MOCKS_H_

#include "base/macros.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockWorkItem : public WorkItem {
 public:
  MockWorkItem();
  ~MockWorkItem();

  MOCK_METHOD0(DoImpl, bool());
  MOCK_METHOD0(RollbackImpl, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWorkItem);
};

class MockCondition : public WorkItem::Condition {
 public:
  MockCondition();
  ~MockCondition();

  MOCK_CONST_METHOD0(ShouldRun, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCondition);
};

using StrictMockWorkItem = testing::StrictMock<MockWorkItem>;
using StrictMockCondition = testing::StrictMock<MockCondition>;

#endif  // CHROME_INSTALLER_UTIL_WORK_ITEM_MOCKS_H_
