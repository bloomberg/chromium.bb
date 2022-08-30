// Copyright 2022 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>
#include <fp16/fp16.h>
#include "bench/utils.h"
#include <xnnpack/aligned-allocator.h>
#include <xnnpack/common.h>
#include <xnnpack/params.h>
#include <xnnpack/params-init.h>
#include <xnnpack/raddstoreexpminusmax.h>
#include <xnnpack/rmax.h>


static void f16_raddstoreexpminusmax(
  benchmark::State& state,
  xnn_f16_rmax_ukernel_function rmax,
  xnn_f16_raddstoreexpminusmax_ukernel_function raddstoreexpminusmax,
  xnn_init_f16_expminus_params_fn init_params,
  benchmark::utils::IsaCheckFunction isa_check = nullptr)
{
  if (isa_check && !isa_check(state)) {
    return;
  }

  const size_t elements = state.range(0);
  const size_t cache_line_size_max = 128;
  const size_t packed_elements = benchmark::utils::RoundUp(elements, cache_line_size_max / sizeof(uint16_t));

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto f32rng = std::bind(std::uniform_real_distribution<float>(-100.0f, 100.0f), std::ref(rng));
  auto f16rng = std::bind(fp16_ieee_from_fp32_value, f32rng);

  const size_t num_buffers = 1 +
    benchmark::utils::DivideRoundUp<size_t>(benchmark::utils::GetMaxCacheSize(), packed_elements * sizeof(uint16_t));
  std::vector<uint16_t, AlignedAllocator<uint16_t, 64>> x(elements);
  std::vector<uint16_t, AlignedAllocator<uint16_t, 64>> y(packed_elements * num_buffers);

  std::generate(x.begin(), x.end(), std::ref(f16rng));

  benchmark::utils::DisableDenormals();

  xnn_f16_expminus_params params;
  init_params(&params);

  size_t buffer_index = 0;
  for (auto _ : state) {
    state.PauseTiming();
    uint16_t x_max = UINT16_C(0x7E00) /* NaN */;
    rmax(elements * sizeof(uint16_t), x.data(), &x_max);
    if (++buffer_index == num_buffers) {
      buffer_index = 0;
    }
    state.ResumeTiming();

    uint16_t y_sum = UINT16_C(0x7E00) /* NaN */;
    raddstoreexpminusmax(elements * sizeof(uint16_t), x.data(), &x_max, y.data() + buffer_index * packed_elements, &y_sum, &params);
  }

  const uint64_t cpu_frequency = benchmark::utils::GetCurrentCpuFrequency();
  if (cpu_frequency != 0) {
    state.counters["cpufreq"] = cpu_frequency;
  }

  const size_t elements_per_iteration = elements;
  state.counters["elements"] =
    benchmark::Counter(uint64_t(state.iterations()) * elements_per_iteration, benchmark::Counter::kIsRate);

  const size_t bytes_per_iteration = 2 * elements * sizeof(uint16_t);
  state.counters["bytes"] =
    benchmark::Counter(uint64_t(state.iterations()) * bytes_per_iteration, benchmark::Counter::kIsRate);
}

#if XNN_ARCH_ARM64
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x32,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x32,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x32_acc2,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x32_acc2,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x32_acc4,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x32_acc4,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x40,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x40,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x40_acc2,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x40_acc2,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x40_acc5,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x40_acc5,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x48,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x48,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x48_acc2,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x48_acc2,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x48_acc3,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x48_acc3,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x64,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x64,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x64_acc2,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x64_acc2,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x64_acc4,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x64_acc4,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x72,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x72,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x72_acc3,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x72_acc3,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x80,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x80,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x80_acc2,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x80_acc2,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x80_acc5,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x80_acc5,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x96,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x96,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x96_acc2,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x96_acc2,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x96_acc3,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x96_acc3,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, neonfp16arith_rr2_p2_x96_acc6,
                    xnn_f16_rmax_ukernel__neonfp16arith,
                    xnn_f16_raddstoreexpminusmax_ukernel__neonfp16arith_rr2_p2_x96_acc6,
                    xnn_init_f16_expminus_neonfp16arith_rr2_p2_params,
                    benchmark::utils::CheckNEONFP16ARITH)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
#endif  // XNN_ARCH_ARM64

#if XNN_ARCH_X86 || XNN_ARCH_X86_64
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x32,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x32,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x32_acc2,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x32_acc2,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x32_acc4,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x32_acc4,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x40,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x40,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x40_acc2,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x40_acc2,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x40_acc5,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x40_acc5,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x48,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x48,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x48_acc2,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x48_acc2,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x48_acc3,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x48_acc3,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x64,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x64,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x64_acc2,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x64_acc2,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x64_acc4,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x64_acc4,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x72,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x72,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x72_acc3,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x72_acc3,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x80,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x80,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x80_acc2,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x80_acc2,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x80_acc5,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x80_acc5,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x96,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x96,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x96_acc2,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x96_acc2,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x96_acc3,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x96_acc3,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(f16_raddstoreexpminusmax, avx2_rr1_p2_x96_acc6,
                    xnn_f16_rmax_ukernel__f16c,
                    xnn_f16_raddstoreexpminusmax_ukernel__avx2_rr1_p2_x96_acc6,
                    xnn_init_f16_expminus_avx2_rr1_p2_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<uint16_t, uint16_t>)
    ->UseRealTime();
#endif  // XNN_ARCH_X86 || XNN_ARCH_X86_64

#ifndef XNNPACK_BENCHMARK_NO_MAIN
BENCHMARK_MAIN();
#endif
