// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>
#include <vector>

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/sqlite/fuzz/sql_query_grammar.pb.h"
#include "third_party/sqlite/fuzz/sql_query_proto_to_string.h"
#include "third_party/sqlite/fuzz/sql_run_queries.h"

using namespace sql_query_grammar;

// TODO(mpdenton) Fuzzing tasks
// 5. Definitely fix a lot of the syntax errors that SQlite spits out
// 12. CORPUS Indexes on expressions (https://www.sqlite.org/expridx.html) and
// other places using functions on columns???
// 17. Generate a nice big random, well-formed corpus.

// FIXME in the future
// 1. Rest of the pragmas
// 2. Make sure defensive config is off
// 3. Fuzz the recover extension from the third patch
// 5. Temp-file database, for better fuzzing of VACUUM and journalling.

DEFINE_BINARY_PROTO_FUZZER(const SQLQueries& sql_queries) {
  std::vector<std::string> queries = sql_fuzzer::SQLQueriesToVec(sql_queries);

  if (getenv("LPM_DUMP_NATIVE_INPUT") && queries.size() != 0) {
    std::cout << "_________________________" << std::endl;
    for (std::string query : queries) {
      if (query == ";")
        continue;
      std::cout << query << std::endl;
    }
    std::cout << "------------------------" << std::endl;
  }

  sql_fuzzer::RunSqlQueries(queries);
}
