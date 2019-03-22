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

void LogThroughputAndLatency(size_t chunk_size,
                             size_t chunk_count,
                             base::TimeTicks tick,
                             base::TimeTicks tock) {
  size_t total_size = chunk_size * chunk_count;
  double elapsed_us = (tock - tick).InMicrosecondsF();
  double throughput = total_size / elapsed_us;
  double latency_us = elapsed_us / chunk_count;
  LOG(INFO) << "  Throughput = " << throughput << "MB/s";
  LOG(INFO) << "  Latency (size = " << chunk_size << ") = " << latency_us
            << "us";
  LOG(INFO) << "AS_CSV=" << chunk_size << "," << throughput << ","
            << latency_us;
}

void CompressChunks(const std::string& contents,
                    size_t chunk_size,
                    bool snappy,
                    std::vector<std::string>* compressed_chunks) {
  CHECK(compressed_chunks);
  size_t chunk_count = contents.size() / chunk_size;

  for (size_t i = 0; i < chunk_count; ++i) {
    std::string compressed;
    base::StringPiece input(contents.c_str() + i * chunk_size, chunk_size);
    if (snappy)
      CHECK(snappy::Compress(input.data(), input.size(), &compressed));
    else
      CHECK(compression::GzipCompress(input, &compressed));

    compressed_chunks->push_back(compressed);
  }
}

void BenchmarkDecompression(const std::string& contents,
                            size_t chunk_size,
                            bool snappy) {
  std::vector<std::string> compressed_chunks;
  CompressChunks(contents, chunk_size, snappy, &compressed_chunks);

  auto tick = base::TimeTicks::Now();
  for (const auto& chunk : compressed_chunks) {
    std::string uncompressed;
    if (snappy) {
      snappy::Uncompress(chunk.c_str(), chunk.size(), &uncompressed);
    } else {
      CHECK(compression::GzipUncompress(chunk, &uncompressed));
    }
  }
  auto tock = base::TimeTicks::Now();

  LogThroughputAndLatency(chunk_size, compressed_chunks.size(), tick, tock);
}

void BenchmarkCompression(const std::string& contents,
                          size_t chunk_size,
                          bool snappy) {
  auto tick = base::TimeTicks::Now();
  std::vector<std::string> compressed_chunks;
  CompressChunks(contents, chunk_size, snappy, &compressed_chunks);
  auto tock = base::TimeTicks::Now();

  size_t compressed_size = 0;
  for (const auto& compressed_chunk : compressed_chunks)
    compressed_size += compressed_chunk.size();

  double ratio = contents.size() / static_cast<double>(compressed_size);
  LOG(INFO) << "  Compression ratio = " << ratio;
  LogThroughputAndLatency(chunk_size, compressed_chunks.size(), tick, tock);
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

  // Make sure we have at least 40MiB.
  constexpr size_t kPageSize = 1 << 12;
  constexpr size_t target_size = 40 * 1024 * 1024;
  std::string repeated_contents;
  size_t repeats = target_size / contents.size() + 1;

  repeated_contents.reserve(repeats * contents.size());
  for (size_t i = 0; i < repeats; ++i)
    repeated_contents.append(contents);

  for (bool use_snappy : {false, true}) {
    LOG(INFO) << "\n\n\n\n" << (use_snappy ? "Snappy" : "Gzip");
    for (size_t size = kPageSize; size < contents.size(); size *= 2) {
      LOG(INFO) << "Size = " << size;
      LOG(INFO) << "Compression";
      BenchmarkCompression(repeated_contents, size, use_snappy);
      LOG(INFO) << "Decompression";
      BenchmarkDecompression(repeated_contents, size, use_snappy);
    }
  }
  return 0;
}
