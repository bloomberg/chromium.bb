/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/framework/function_handle_cache.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/resource_mgr.h"
#include "tensorflow/core/kernels/data/dataset_utils.h"
#include "tensorflow/core/kernels/ops_util.h"
#include "tensorflow/core/lib/core/threadpool.h"
#include "tensorflow/core/lib/io/record_writer.h"
#include "tensorflow/core/platform/file_system.h"
#include "tensorflow/core/platform/resource.h"

namespace tensorflow {
namespace data {
namespace experimental {
namespace {

class ToTFRecordOp : public AsyncOpKernel {
 public:
  explicit ToTFRecordOp(OpKernelConstruction* ctx)
      : AsyncOpKernel(ctx),
        background_worker_(ctx->env(), "tf_data_to_tf_record") {}

  template <typename T>
  Status ParseScalarArgument(OpKernelContext* ctx,
                             const StringPiece& argument_name, T* output) {
    const Tensor* argument_t;
    TF_RETURN_IF_ERROR(ctx->input(argument_name, &argument_t));
    if (!TensorShapeUtils::IsScalar(argument_t->shape())) {
      return errors::InvalidArgument(argument_name, " must be a scalar");
    }
    *output = argument_t->scalar<T>()();
    return Status::OK();
  }

  void ComputeAsync(OpKernelContext* ctx, DoneCallback done) override {
    // The call to `iterator->GetNext()` may block and depend on an inter-op
    // thread pool thread, so we issue the call using a background thread.
    background_worker_.Schedule([this, ctx, done = std::move(done)]() {
      OP_REQUIRES_OK_ASYNC(ctx, DoCompute(ctx), done);
      done();
    });
  }

 private:
  Status DoCompute(OpKernelContext* ctx) {
    tensorflow::ResourceTagger tag(kTFDataResourceTag,
                                   ctx->op_kernel().type_string());
    tstring filename;
    TF_RETURN_IF_ERROR(
        ParseScalarArgument<tstring>(ctx, "filename", &filename));
    tstring compression_type;
    TF_RETURN_IF_ERROR(ParseScalarArgument<tstring>(ctx, "compression_type",
                                                    &compression_type));
    std::unique_ptr<WritableFile> file;
    TF_RETURN_IF_ERROR(ctx->env()->NewWritableFile(filename, &file));
    auto writer = absl::make_unique<io::RecordWriter>(
        file.get(),
        io::RecordWriterOptions::CreateRecordWriterOptions(compression_type));

    DatasetBase* dataset;
    TF_RETURN_IF_ERROR(GetDatasetFromVariantTensor(ctx->input(0), &dataset));

    IteratorContext::Params params(ctx);
    FunctionHandleCache function_handle_cache(params.flr);
    params.function_handle_cache = &function_handle_cache;
    ResourceMgr resource_mgr;
    params.resource_mgr = &resource_mgr;
    CancellationManager cancellation_manager(ctx->cancellation_manager());
    params.cancellation_manager = &cancellation_manager;

    IteratorContext iter_ctx(std::move(params));
    std::unique_ptr<IteratorBase> iterator;
    TF_RETURN_IF_ERROR(dataset->MakeIterator(
        &iter_ctx, /*parent=*/nullptr, "ToTFRecordOpIterator", &iterator));

    std::vector<Tensor> components;
    components.reserve(dataset->output_dtypes().size());
    bool end_of_sequence;
    do {
      TF_RETURN_IF_ERROR(
          iterator->GetNext(&iter_ctx, &components, &end_of_sequence));

      if (!end_of_sequence) {
        TF_RETURN_IF_ERROR(
            writer->WriteRecord(components[0].scalar<tstring>()()));
      }
      components.clear();
    } while (!end_of_sequence);
    return Status::OK();
  }

  BackgroundWorker background_worker_;
};

REGISTER_KERNEL_BUILDER(Name("DatasetToTFRecord").Device(DEVICE_CPU),
                        ToTFRecordOp);
REGISTER_KERNEL_BUILDER(
    Name("ExperimentalDatasetToTFRecord").Device(DEVICE_CPU), ToTFRecordOp);

}  // namespace
}  // namespace experimental
}  // namespace data
}  // namespace tensorflow
