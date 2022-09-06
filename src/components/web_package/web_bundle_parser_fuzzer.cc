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
#include "components/web_package/web_bundle_parser.h"
#include "components/web_package/web_bundle_parser_factory.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace {

class DataSource : public web_package::mojom::BundleDataSource {
 public:
  DataSource(const bool is_random_access_context, const std::string& data)
      : is_random_access_context_(is_random_access_context), data_(data) {}

  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    if (offset >= data_.size()) {
      std::move(callback).Run(absl::nullopt);
      return;
    }
    const auto start = data_.begin() + offset;
    length = std::min(length, data_.size() - offset);
    std::move(callback).Run(std::vector<uint8_t>(start, start + length));
  }

  void Length(LengthCallback callback) override {
    std::move(callback).Run(data_.size());
  }

  void IsRandomAccessContext(IsRandomAccessContextCallback callback) override {
    std::move(callback).Run(is_random_access_context_);
  }

  void AddReceiver(
      mojo::PendingReceiver<web_package::mojom::BundleDataSource> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  bool is_random_access_context_;
  const std::string data_;
  mojo::ReceiverSet<web_package::mojom::BundleDataSource> receivers_;
};

class WebBundleParserFuzzer {
 public:
  WebBundleParserFuzzer(const bool is_random_access_context,
                        const std::string& data)
      : data_source_(is_random_access_context, data) {}

  void FuzzBundle(base::RunLoop* run_loop) {
    mojo::PendingRemote<web_package::mojom::BundleDataSource>
        data_source_remote;
    data_source_.AddReceiver(
        data_source_remote.InitWithNewPipeAndPassReceiver());

    web_package::WebBundleParserFactory factory_impl;
    web_package::mojom::WebBundleParserFactory& factory = factory_impl;
    factory.GetParserForDataSource(parser_.BindNewPipeAndPassReceiver(),
                                   std::move(data_source_remote));

    quit_loop_ = run_loop->QuitClosure();
    parser_->ParseMetadata(base::BindOnce(
        &WebBundleParserFuzzer::OnParseMetadata, base::Unretained(this)));
  }

  void OnParseMetadata(web_package::mojom::BundleMetadataPtr metadata,
                       web_package::mojom::BundleMetadataParseErrorPtr error) {
    if (!metadata) {
      std::move(quit_loop_).Run();
      return;
    }
    for (auto& item : metadata->requests)
      locations_.push_back(std::move(item.second));
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
                       web_package::mojom::BundleResponsePtr response,
                       web_package::mojom::BundleResponseParseErrorPtr error) {
    ParseResponses(index + 1);
  }

 private:
  mojo::Remote<web_package::mojom::WebBundleParser> parser_;
  DataSource data_source_;
  base::OnceClosure quit_loop_;
  std::vector<web_package::mojom::BundleResponseLocationPtr> locations_;
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

  std::string web_bundle(reinterpret_cast<const char*>(data), size);
  bool is_random_access_context = std::hash<std::string>()(web_bundle) & 1;

  WebBundleParserFuzzer fuzzer(is_random_access_context, web_bundle);
  base::RunLoop run_loop;
  env->task_executor.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebBundleParserFuzzer::FuzzBundle,
                                base::Unretained(&fuzzer), &run_loop));
  run_loop.Run();

  return 0;
}
