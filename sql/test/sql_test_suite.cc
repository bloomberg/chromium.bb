// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/sql_test_suite.h"

#include "base/metrics/statistics_recorder.h"
#include "sql/test/paths.h"

namespace sql {

SQLTestSuite::SQLTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

SQLTestSuite::~SQLTestSuite() {}

void SQLTestSuite::Initialize() {
  base::TestSuite::Initialize();

  // Initialize the histograms subsystem, so that any histograms hit in tests
  // are correctly registered with the statistics recorder and can be queried
  // by tests.
  base::StatisticsRecorder::Initialize();

  sql::test::RegisterPathProvider();
}

void SQLTestSuite::Shutdown() {
  base::TestSuite::Shutdown();
}

}  // namespace sql
