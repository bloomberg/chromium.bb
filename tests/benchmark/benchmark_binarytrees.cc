// Copyright 2014 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "native_client/tests/benchmark/framework.h"


extern "C" {
  int main_binarytrees(int argc, const char* argv[]);
}

namespace {

// Wrap BinaryTrees in benchmark harness
class BenchmarkBinaryTrees : public Benchmark {
 public:
  virtual int Run() {
    const char *args[] = {"binarytrees", "12"};
    return main_binarytrees(1, args);
  }
  virtual const std::string Name() { return "BinaryTrees"; }
  virtual const std::string Notes() { return "c version"; }
};

}  // namespace

// Register an instance to the list of benchmarks to be run.
RegisterBenchmark<BenchmarkBinaryTrees> benchmark_binarytrees;
