/* Copyright 2019 Google LLC. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <cstdio>
#include <cstdlib>
#include <string>

#include "ruy/test.h"

namespace ruy {

using LhsScalar = RUY_TEST_LHSSCALAR;
using RhsScalar = RUY_TEST_RHSSCALAR;
using AccumScalar = RUY_TEST_ACCUMSCALAR;
using DstScalar = RUY_TEST_DSTSCALAR;
using TestSetType = TestSet<LhsScalar, RhsScalar, AccumScalar, DstScalar>;

struct BenchmarkShape {
  int rows;
  int depth;
  int cols;
  int symm_lhs;
  int symm_rhs;
};

template <typename TestSetType>
std::vector<std::unique_ptr<TestResult<DstScalar>>> Benchmark(
    const BenchmarkShape& shape) {
  TestSetType test_set;
  test_set.rows = shape.rows;
  test_set.depth = shape.depth;
  test_set.cols = shape.cols;
  const char* orders = "RCC";
  const char* orders_env = getenv("ORDERS");
  if (orders_env) {
    bool error = false;
    if (strlen(orders_env) != 3) {
      error = true;
    } else {
      for (int i = 0; i < 3; i++) {
        if (orders_env[i] != 'R' && orders_env[i] != 'C') {
          error = true;
        }
      }
    }
    if (error) {
      fprintf(stderr,
              "ORDERS must contain 3 letters, each either R or C, indicating "
              "whether to use Row-major or Column-major storage order for the "
              "LHS, RHS and Destination matrix.\n");
      exit(EXIT_FAILURE);
    }
    orders = orders_env;
  }
  test_set.lhs_order = orders[0] == 'R' ? Order::kRowMajor : Order::kColMajor;
  test_set.rhs_order = orders[1] == 'R' ? Order::kRowMajor : Order::kColMajor;
  test_set.dst_order = orders[2] == 'R' ? Order::kRowMajor : Order::kColMajor;
  test_set.layout_style = LayoutStyle::kUnstridedLinear;
  test_set.benchmark = true;
  const int asymmetry_lhs = shape.symm_lhs ? 0 : 1;
  const int asymmetry_rhs = shape.symm_rhs ? 0 : 1;
  test_set.lhs_zero_point = SymmetricZeroPoint<LhsScalar>() + asymmetry_lhs;
  test_set.rhs_zero_point = SymmetricZeroPoint<RhsScalar>() + asymmetry_rhs;
  test_set.use_specified_zero_points = true;
  test_set.perchannel = GetBoolEnvVarOrFalse("PERCHANNEL");
  if (getenv("PREPACK_LHS") || getenv("PREPACK_RHS")) {
    fprintf(stderr,
            "PREPACK_LHS and PREPACK_RHS are deprecated. Use CACHE_LHS and "
            "CACHE_RHS instead.\n");
    exit(EXIT_FAILURE);
  }
  test_set.cache_lhs = GetBoolEnvVarOrFalse("CACHE_LHS");
  test_set.cache_rhs = GetBoolEnvVarOrFalse("CACHE_RHS");
  test_set.Run();
  return std::move(test_set.results);
}

std::vector<int> ParseCommaSeparatedInts(
    const std::string& comma_separated_ints) {
  std::vector<int> result;
  for (std::size_t pos = 0; pos < comma_separated_ints.size();) {
    std::size_t delim_pos = comma_separated_ints.find(',', pos);
    if (delim_pos == std::string::npos) {
      delim_pos = comma_separated_ints.size();
    }
    result.push_back(
        std::stoi(comma_separated_ints.substr(pos, delim_pos - pos)));
    pos = delim_pos + 1;
  }
  return result;
}

void Benchmark() {
  // For now, support for int8*int16 cases is limited to the
  // symmetric case (zero_point==0) because that appears to be
  // the case in the initial use cases, and that limits complexity
  // in thinking about accumulator overflows. This would not be a concern
  // in the future if the accumulator type was int64, but for now its int32.
  const bool is_int8_times_int16 =
      (std::is_same<LhsScalar, std::int8_t>::value &&
       std::is_same<RhsScalar, std::int16_t>::value) ||
      (std::is_same<LhsScalar, std::int16_t>::value &&
       std::is_same<RhsScalar, std::int8_t>::value);
  const bool symm_lhs = std::is_floating_point<LhsScalar>::value ||
                        is_int8_times_int16 || GetBoolEnvVarOrFalse("SYMM_LHS");
  const bool symm_rhs = std::is_floating_point<RhsScalar>::value ||
                        is_int8_times_int16 || GetBoolEnvVarOrFalse("SYMM_RHS");
  const bool benchmark_cubic = GetBoolEnvVarOrFalse("RUY_BENCHMARK_CUBIC") ||
                               GetBoolEnvVarOrFalse("RUY_BENCHMARK_CUBIC_LIST");
  const int explicit_rows = GetIntEnvVarOrZero("ROWS");
  const int explicit_cols = GetIntEnvVarOrZero("COLS");
  const int explicit_depth = GetIntEnvVarOrZero("DEPTH");

  std::vector<BenchmarkShape> shapes;

  if (benchmark_cubic) {
    std::vector<int> sizes;
    const char* benchmark_cubic_list_env = getenv("RUY_BENCHMARK_CUBIC_LIST");
    if (benchmark_cubic_list_env) {
      sizes = ParseCommaSeparatedInts(benchmark_cubic_list_env);
    } else {
      // Often 8 is used for this multiplier, but to check teeny sizes one can
      // use 1.
      static constexpr int cubic_size_multiplier = 8;
      for (int i = 2 * cubic_size_multiplier;
           i <= (512 * cubic_size_multiplier); i *= 2) {
        sizes.push_back(i);
        if (i < (512 * cubic_size_multiplier)) {
          sizes.push_back(i * 3 / 2);
        }
      }
    }
    for (int i : sizes) {
      BenchmarkShape shape;
      // Even in cubic mode, one may still override an individual dimension
      // to allow testing a batch of rectangular sizes.
      shape.rows = explicit_rows ? explicit_rows : i;
      shape.cols = explicit_cols ? explicit_cols : i;
      shape.depth = explicit_depth ? explicit_depth : i;
      shape.symm_lhs = symm_lhs;
      shape.symm_rhs = symm_rhs;
      shapes.push_back(shape);
    }
  } else {
    BenchmarkShape shape;
    shape.rows = explicit_rows;
    shape.cols = explicit_cols;
    shape.depth = explicit_depth;
    if (!shape.rows || !shape.depth || !shape.cols) {
      fprintf(stderr,
              "Please specify positive sizes with these env vars: ROWS, DEPTH, "
              "COLS.\n");
      exit(1);
    }
    shape.symm_lhs = symm_lhs;
    shape.symm_rhs = symm_rhs;
    shapes.push_back(shape);
  }

  for (int i = 0; i < static_cast<int>(shapes.size()); i++) {
    const auto& shape = shapes[i];
    const auto& results = Benchmark<TestSetType>(shape);
    if (i == 0) {
      if (benchmark_cubic) {
        printf("size");
        for (const auto& result : results) {
          if (results.size() > 1) {
            printf(",%s:Gop/s", PathName(*result).c_str());
          } else {
            printf(",Gop/s");
          }
          if (GetBoolEnvVarOrFalse("RUY_BENCHMARK_PMU")) {
            printf(
                ",l1_refill,l2_refill,l3_refill,l1tlb_refill,l2tlb_refill,"
                "mispred,frontend_stall,backend_stall");
          }
        }
        printf("\n");
      } else {
        printf("path,shape,Gop/s\n");
      }
      fflush(stdout);
    }
    if (benchmark_cubic) {
      printf("%d", shape.rows);
      for (const auto& result : results) {
        printf(",%.4g", 2.0e-9 * shape.rows * shape.cols * shape.depth /
                            result->latency);
        if (GetBoolEnvVarOrFalse("RUY_BENCHMARK_PMU")) {
          printf(",%.3g,%.3g,%.3g,%.3g,%.3g,%.3g,%.3g,%.3g",
                 result->l1_refill_rate, result->l2_refill_rate,
                 result->l3_refill_rate, result->l1tlb_refill_rate,
                 result->l2tlb_refill_rate, result->mispred_rate,
                 result->frontend_stall_rate, result->backend_stall_rate);
        }
      }
      printf("\n");
      fflush(stdout);
    } else {
      for (const auto& result : results) {
        printf(
            "%s,%dx%dx%d,%.4g", PathName(*result).c_str(), shape.rows,
            shape.depth, shape.cols,
            2.0e-9 * shape.rows * shape.cols * shape.depth / result->latency);
        if (GetBoolEnvVarOrFalse("RUY_BENCHMARK_PMU")) {
          printf(",%.3g,%.3g,%.3g,%.3g,%.3g,%.3g,%.3g,%.3g",
                 result->l1_refill_rate, result->l2_refill_rate,
                 result->l3_refill_rate, result->l1tlb_refill_rate,
                 result->l2tlb_refill_rate, result->mispred_rate,
                 result->frontend_stall_rate, result->backend_stall_rate);
        }
        printf("\n");
      }
      fflush(stdout);
    }
  }
}

}  // namespace ruy

int main() { ruy::Benchmark(); }
