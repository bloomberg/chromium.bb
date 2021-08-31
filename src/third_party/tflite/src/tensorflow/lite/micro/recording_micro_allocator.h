/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_LITE_MICRO_RECORDING_MICRO_ALLOCATOR_H_
#define TENSORFLOW_LITE_MICRO_RECORDING_MICRO_ALLOCATOR_H_

#include "tensorflow/lite/micro/compatibility.h"
#include "tensorflow/lite/micro/micro_allocator.h"
#include "tensorflow/lite/micro/recording_simple_memory_allocator.h"

namespace tflite {

// List of buckets currently recorded by this class. Each type keeps a list of
// allocated information during model initialization.
enum class RecordedAllocationType {
  kTfLiteTensorArray,
  kTfLiteTensorArrayQuantizationData,
  kTfLiteTensorVariableBufferData,
  kNodeAndRegistrationArray,
  kOpData,
};

// Container for holding information about allocation recordings by a given
// type. Each recording contains the number of bytes requested, the actual bytes
// allocated (can defer from requested by alignment), and the number of items
// allocated.
typedef struct RecordedAllocation {
  RecordedAllocation() : requested_bytes(0), used_bytes(0), count(0) {}
  size_t requested_bytes;
  size_t used_bytes;
  size_t count;
} RecordedAllocation;

// Utility subclass of MicroAllocator that records all allocations
// inside the arena. A summary of allocations can be logged through the
// ErrorReporter by invoking LogAllocations(). This special allocator requires
// an instance of RecordingSimpleMemoryAllocator to capture allocations in the
// head and tail. Arena allocation recording can be retrieved by type through
// the GetRecordedAllocation() function. This class should only be used for
// auditing memory usage or integration testing.
class RecordingMicroAllocator : public MicroAllocator {
 public:
  static RecordingMicroAllocator* Create(uint8_t* tensor_arena,
                                         size_t arena_size,
                                         ErrorReporter* error_reporter);

  // Returns the recorded allocations information for a given allocation type.
  RecordedAllocation GetRecordedAllocation(
      RecordedAllocationType allocation_type) const;

  const RecordingSimpleMemoryAllocator* GetSimpleMemoryAllocator() const;

  // Logs out through the ErrorReporter all allocation recordings by type
  // defined in RecordedAllocationType.
  void PrintAllocations() const;

 protected:
  TfLiteStatus AllocateTfLiteTensorArray(TfLiteContext* context,
                                         const SubGraph* subgraph) override;
  TfLiteStatus PopulateTfLiteTensorArrayFromFlatbuffer(
      const Model* model, TfLiteContext* context,
      const SubGraph* subgraph) override;
  TfLiteStatus AllocateNodeAndRegistrations(
      const SubGraph* subgraph,
      NodeAndRegistration** node_and_registrations) override;
  TfLiteStatus PrepareNodeAndRegistrationDataFromFlatbuffer(
      const Model* model, const SubGraph* subgraph,
      const MicroOpResolver& op_resolver,
      NodeAndRegistration* node_and_registrations) override;
  TfLiteStatus AllocateVariables(TfLiteContext* context,
                                 const SubGraph* subgraph) override;

  void SnapshotAllocationUsage(RecordedAllocation& recorded_allocation);
  void RecordAllocationUsage(RecordedAllocation& recorded_allocation);

 private:
  RecordingMicroAllocator(RecordingSimpleMemoryAllocator* memory_allocator,
                          ErrorReporter* error_reporter);

  void PrintRecordedAllocation(RecordedAllocationType allocation_type,
                               const char* allocation_name,
                               const char* allocation_description) const;

  const RecordingSimpleMemoryAllocator* recording_memory_allocator_;

  RecordedAllocation recorded_tflite_tensor_array_data_;
  RecordedAllocation recorded_tflite_tensor_array_quantization_data_;
  RecordedAllocation recorded_tflite_tensor_variable_buffer_data_;
  RecordedAllocation recorded_node_and_registration_array_data_;
  RecordedAllocation recorded_op_data_;

  TF_LITE_REMOVE_VIRTUAL_DELETE
};

}  // namespace tflite

#endif  // TENSORFLOW_LITE_MICRO_RECORDING_MICRO_ALLOCATOR_H_
