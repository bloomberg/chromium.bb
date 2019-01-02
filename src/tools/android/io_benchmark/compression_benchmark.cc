// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Benchmarks in-memory compression and decompression of an input file,
// comparing zlib and snappy.

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/snappy/src/snappy.h"
#include "third_party/zlib/google/compression_utils.h"

namespace {

void LogThroughputAndLatency(size_t size,
                             int repeats,
                             base::TimeTicks tick,
                             base::TimeTicks tock) {
  size_t total_size = size * repeats;
  double elapsed_us = (tock - tick).InMicrosecondsF();
  double throughput = total_size / elapsed_us;
  double latency_us = elapsed_us / repeats;
  LOG(INFO) << "  Throughput = " << throughput << "MB/s";
  LOG(INFO) << "  Latency (size = " << size << ") = " << latency_us << "us";
  LOG(INFO) << size << "," << throughput << "," << latency_us;
}

void BenchmarkDecompression(const std::string& contents,
                            int repeats,
                            bool snappy) {
  std::string compressed;
  if (snappy) {
    snappy::Compress(contents.c_str(), contents.size(), &compressed);
  } else {
    CHECK(compression::GzipCompress(contents, &compressed));
  }

  auto tick = base::TimeTicks::Now();
  for (int i = 0; i < repeats; ++i) {
    std::string uncompressed;
    if (snappy) {
      snappy::Uncompress(compressed.c_str(), compressed.size(), &uncompressed);
    } else {
      CHECK(compression::GzipUncompress(compressed, &uncompressed));
    }
  }
  auto tock = base::TimeTicks::Now();

  LogThroughputAndLatency(contents.size(), repeats, tick, tock);
}

void BenchmarkCompression(const std::string& contents,
                          int repeats,
                          bool snappy) {
  size_t compressed_size = 0;
  auto tick = base::TimeTicks::Now();
  for (int i = 0; i < repeats; ++i) {
    std::string compressed;
    if (snappy) {
      compressed_size =
          snappy::Compress(contents.c_str(), contents.size(), &compressed);
    } else {
      CHECK(compression::GzipCompress(contents, &compressed));
      compressed_size = compressed.size();
    }
  }
  auto tock = base::TimeTicks::Now();

  double ratio = contents.size() / static_cast<double>(compressed_size);
  LOG(INFO) << "  Compression ratio = " << ratio;
  LogThroughputAndLatency(contents.size(), repeats, tick, tock);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    LOG(FATAL) << "Usage: " << argv[0] << " <filename>\n\n"
               << "Where the file contains data to compress";
    return 1;
  }

  LOG(INFO) << "Reading the input file";
  auto path = base::FilePath(std::string(argv[1]));
  std::string contents;
  CHECK(base::ReadFileToString(path, &contents));

  constexpr size_t kPageSize = 1 << 12;
  for (bool use_snappy : {false, true}) {
    LOG(INFO) << "\n\n\n\n" << (use_snappy ? "Snappy" : "Gzip");
    for (size_t size = kPageSize; size < contents.size() * 2; size *= 2) {
      size_t actual_size = std::min(contents.size(), size);
      std::string data = contents.substr(0, actual_size);
      LOG(INFO) << "Size = " << actual_size;
      LOG(INFO) << "Compression";
      BenchmarkCompression(data, (10 * 1024 * kPageSize) / actual_size,
                           use_snappy);  // 40MiB.
      LOG(INFO) << "Decompression";
      BenchmarkDecompression(data, 100, use_snappy);
    }
  }
  return 0;
}
