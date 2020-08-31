// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/i18n/icu_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/web_bundle_parser.h"
#include "services/data_decoder/web_bundle_parser_factory.h"

namespace {

class DataSource : public data_decoder::mojom::BundleDataSource {
 public:
  DataSource(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    if (offset >= size_) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    const uint8_t* start = data_ + offset;
    length = std::min(length, size_ - offset);
    std::move(callback).Run(std::vector<uint8_t>(start, start + length));
  }

  void AddReceiver(
      mojo::PendingReceiver<data_decoder::mojom::BundleDataSource> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  const uint8_t* data_;
  size_t size_;
  mojo::ReceiverSet<data_decoder::mojom::BundleDataSource> receivers_;
};

class WebBundleParserFuzzer {
 public:
  WebBundleParserFuzzer(const uint8_t* data, size_t size)
      : data_source_(data, size) {}

  void FuzzBundle(base::RunLoop* run_loop) {
    mojo::PendingRemote<data_decoder::mojom::BundleDataSource>
        data_source_remote;
    data_source_.AddReceiver(
        data_source_remote.InitWithNewPipeAndPassReceiver());

    data_decoder::WebBundleParserFactory factory_impl;
    data_decoder::mojom::WebBundleParserFactory& factory = factory_impl;
    factory.GetParserForDataSource(parser_.BindNewPipeAndPassReceiver(),
                                   std::move(data_source_remote));

    quit_loop_ = run_loop->QuitClosure();
    parser_->ParseMetadata(base::BindOnce(
        &WebBundleParserFuzzer::OnParseMetadata, base::Unretained(this)));
  }

  void OnParseMetadata(data_decoder::mojom::BundleMetadataPtr metadata,
                       data_decoder::mojom::BundleMetadataParseErrorPtr error) {
    if (!metadata) {
      std::move(quit_loop_).Run();
      return;
    }
    for (const auto& item : metadata->requests) {
      for (auto& resp_location : item.second->response_locations)
        locations_.push_back(std::move(resp_location));
    }
    ParseResponses(0);
  }

  void ParseResponses(size_t index) {
    if (index >= locations_.size()) {
      std::move(quit_loop_).Run();
      return;
    }

    parser_->ParseResponse(
        locations_[index]->offset, locations_[index]->length,
        base::BindOnce(&WebBundleParserFuzzer::OnParseResponse,
                       base::Unretained(this), index));
  }

  void OnParseResponse(size_t index,
                       data_decoder::mojom::BundleResponsePtr response,
                       data_decoder::mojom::BundleResponseParseErrorPtr error) {
    ParseResponses(index + 1);
  }

 private:
  mojo::Remote<data_decoder::mojom::WebBundleParser> parser_;
  DataSource data_source_;
  base::OnceClosure quit_loop_;
  std::vector<data_decoder::mojom::BundleResponseLocationPtr> locations_;
};

struct Environment {
  Environment() {
    mojo::core::Init();
    CHECK(base::i18n::InitializeICU());
  }

  // Used by ICU integration.
  base::AtExitManager at_exit_manager;
  base::SingleThreadTaskExecutor task_executor;
};

}  // namespace

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment* env = new Environment();

  WebBundleParserFuzzer fuzzer(data, size);
  base::RunLoop run_loop;
  env->task_executor.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebBundleParserFuzzer::FuzzBundle,
                                base::Unretained(&fuzzer), &run_loop));
  run_loop.Run();

  return 0;
}
