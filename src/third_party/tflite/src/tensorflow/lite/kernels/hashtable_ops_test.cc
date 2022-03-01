/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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
#include <initializer_list>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "tensorflow/lite/experimental/resource/lookup_interfaces.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow/lite/kernels/test_util.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/testing/util.h"

namespace tflite {

// Forward declaration for op kernels.
namespace ops {
namespace builtin {

TfLiteRegistration* Register_HASHTABLE();
TfLiteRegistration* Register_HASHTABLE_FIND();
TfLiteRegistration* Register_HASHTABLE_IMPORT();
TfLiteRegistration* Register_HASHTABLE_SIZE();

}  // namespace builtin
}  // namespace ops

namespace {

using ::testing::ElementsAreArray;

typedef enum {
  kResourceTensorId = 0,
  kKeyTensorId = 1,
  kValueTensorId = 2,
  kQueryTensorId = 3,
  kResultTensorId = 4,
  kSizeTensorId = 5,
  kDefaultValueTensorId = 6,
  kResourceTwoTensorId = 7,
  kKeyTwoTensorId = 8,
  kValueTwoTensorId = 9,
  kQueryTwoTensorId = 10,
  kResultTwoTensorId = 11,
  kSizeTwoTensorId = 12,
  kDefaultValueTwoTensorId = 13,
} TensorIds;

template <typename T>
void SetTensorData(Interpreter* interpreter, int tensorId,
                   std::vector<T> data) {
  auto* tensor = interpreter->tensor(tensorId);
  auto* tensor_data = GetTensorData<T>(tensor);
  int i = 0;
  for (auto item : data) {
    tensor_data[i++] = item;
  }
}

template <>
void SetTensorData(Interpreter* interpreter, int tensorId,
                   std::vector<std::string> data) {
  auto* tensor = interpreter->tensor(tensorId);
  DynamicBuffer buf;
  for (auto item : data) {
    buf.AddString(item.c_str(), item.length());
  }
  buf.WriteToTensorAsVector(tensor);
}

TensorType ConvertTfLiteType(TfLiteType type) {
  // Currently, hashtable kernels support INT64 and STRING types only.
  switch (type) {
    case kTfLiteInt64:
      return TensorType_INT64;
    case kTfLiteString:
      return TensorType_STRING;
    default:
      CHECK(false);  // Not reached.
      return TensorType_MIN;
  }
}

// HashtableGraph generates a graph with hash table ops. This class can create
// the following scenarios:
//
// - Default graph: One hash table resource with import, lookup, and size ops.
// - Graph without any import node
// - Graph with two import nodes
// - Graph has two hash table resources.
//
template <typename KeyType, typename ValueType>
class HashtableGraph {
 public:
  HashtableGraph(TfLiteType key_type, TfLiteType value_type)
      : key_type_(key_type), value_type_(value_type) {
    interpreter_ = absl::make_unique<Interpreter>(&error_reporter_);
    InitOpRegistrations();
  }
  ~HashtableGraph() {}

  void BuildDefaultGraph() {
    TfLiteHashtableParams* hashtable_params = GetHashtableParams();

    int node_index;
    // Hash table node.
    interpreter_->AddNodeWithParameters(
        {}, {kResourceTensorId}, nullptr, 0,
        reinterpret_cast<void*>(hashtable_params), hashtable_registration_,
        &node_index);

    // Hash table import node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId, kKeyTensorId, kValueTensorId}, {}, nullptr, 0,
        nullptr, hashtable_import_registration_, &node_index);

    // Hash table lookup node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId, kQueryTensorId, kDefaultValueTensorId},
        {kResultTensorId}, nullptr, 0, nullptr, hashtable_find_registration_,
        &node_index);

    // Hash table size node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId}, {kSizeTensorId}, nullptr, 0, nullptr,
        hashtable_size_registration_, &node_index);
  }

  void BuildNoImportGraph() {
    TfLiteHashtableParams* hashtable_params = GetHashtableParams();

    int node_index;
    // Hash table node.
    interpreter_->AddNodeWithParameters(
        {}, {kResourceTensorId}, nullptr, 0,
        reinterpret_cast<void*>(hashtable_params), hashtable_registration_,
        &node_index);

    // Hash table lookup node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId, kQueryTensorId, kDefaultValueTensorId},
        {kResultTensorId}, nullptr, 0, nullptr, hashtable_find_registration_,
        &node_index);

    // Hash table size node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId}, {kSizeTensorId}, nullptr, 0, nullptr,
        hashtable_size_registration_, &node_index);
  }

  void BuildImportTwiceGraph() {
    TfLiteHashtableParams* hashtable_params = GetHashtableParams();

    int node_index;
    // Hash table node.
    interpreter_->AddNodeWithParameters(
        {}, {kResourceTensorId}, nullptr, 0,
        reinterpret_cast<void*>(hashtable_params), hashtable_registration_,
        &node_index);

    // Hash table import node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId, kKeyTensorId, kValueTensorId}, {}, nullptr, 0,
        nullptr, hashtable_import_registration_, &node_index);

    // Hash table import node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId, kKeyTensorId, kValueTensorId}, {}, nullptr, 0,
        nullptr, hashtable_import_registration_, &node_index);

    // Hash table lookup node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId, kQueryTensorId, kDefaultValueTensorId},
        {kResultTensorId}, nullptr, 0, nullptr, hashtable_find_registration_,
        &node_index);

    // Hash table size node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId}, {kSizeTensorId}, nullptr, 0, nullptr,
        hashtable_size_registration_, &node_index);
  }

  void BuildTwoHashtablesGraph() {
    TfLiteHashtableParams* hashtable_params = GetHashtableParams();

    int node_index;
    // Hash table node.
    interpreter_->AddNodeWithParameters(
        {}, {kResourceTensorId}, nullptr, 0,
        reinterpret_cast<void*>(hashtable_params), hashtable_registration_,
        &node_index);

    // Hash table import node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId, kKeyTensorId, kValueTensorId}, {}, nullptr, 0,
        nullptr, hashtable_import_registration_, &node_index);

    // Hash table lookup node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId, kQueryTensorId, kDefaultValueTensorId},
        {kResultTensorId}, nullptr, 0, nullptr, hashtable_find_registration_,
        &node_index);

    // Hash table size node.
    interpreter_->AddNodeWithParameters(
        {kResourceTensorId}, {kSizeTensorId}, nullptr, 0, nullptr,
        hashtable_size_registration_, &node_index);

    TfLiteHashtableParams* hashtable_two_params = GetHashtableParams();

    // Hash table two node.
    interpreter_->AddNodeWithParameters(
        {}, {kResourceTwoTensorId}, nullptr, 0,
        reinterpret_cast<void*>(hashtable_two_params), hashtable_registration_,
        &node_index);

    // Hash table two import node.
    interpreter_->AddNodeWithParameters(
        {kResourceTwoTensorId, kKeyTwoTensorId, kValueTwoTensorId}, {}, nullptr,
        0, nullptr, hashtable_import_registration_, &node_index);

    // Hash table two lookup node.
    interpreter_->AddNodeWithParameters(
        {kResourceTwoTensorId, kQueryTwoTensorId, kDefaultValueTwoTensorId},
        {kResultTwoTensorId}, nullptr, 0, nullptr, hashtable_find_registration_,
        &node_index);

    // Hash table two size node.
    interpreter_->AddNodeWithParameters(
        {kResourceTwoTensorId}, {kSizeTwoTensorId}, nullptr, 0, nullptr,
        hashtable_size_registration_, &node_index);
  }

  TfLiteStatus Invoke() { return interpreter_->Invoke(); }

  void SetTable(std::initializer_list<KeyType> keys,
                std::initializer_list<ValueType> values) {
    keys_ = std::vector<KeyType>(keys);
    values_ = std::vector<ValueType>(values);
  }

  void SetTableTwo(std::initializer_list<KeyType> keys,
                   std::initializer_list<ValueType> values) {
    keys_two_ = std::vector<KeyType>(keys);
    values_two_ = std::vector<ValueType>(values);
  }

  void SetQuery(std::initializer_list<KeyType> queries,
                ValueType default_value) {
    queries_ = std::vector<KeyType>(queries);
    default_value_ = default_value;
  }

  void SetQueryForTableTwo(std::initializer_list<KeyType> queries,
                           ValueType default_value) {
    queries_two_ = std::vector<KeyType>(queries);
    default_value_two_ = default_value;
  }

  int64_t GetTableSize() {
    auto* size_tensor = interpreter_->tensor(kSizeTensorId);
    auto size_tensor_shape = GetTensorShape(size_tensor);
    return GetTensorData<int64_t>(size_tensor)[0];
  }

  int64_t GetTableTwoSize() {
    auto* size_tensor = interpreter_->tensor(kSizeTwoTensorId);
    auto size_tensor_shape = GetTensorShape(size_tensor);
    return GetTensorData<int64_t>(size_tensor)[0];
  }

  std::vector<ValueType> GetLookupResult() {
    auto* result_tensor = interpreter_->tensor(kResultTensorId);
    auto result_tensor_shape = GetTensorShape(result_tensor);
    auto* result_tensor_data = GetTensorData<ValueType>(result_tensor);

    int size = result_tensor_shape.FlatSize();
    std::vector<ValueType> result;
    for (int i = 0; i < size; ++i) {
      result.push_back(result_tensor_data[i]);
    }
    return result;
  }

  std::vector<ValueType> GetLookupTwoResult() {
    auto* result_tensor = interpreter_->tensor(kResultTwoTensorId);
    auto result_tensor_shape = GetTensorShape(result_tensor);
    auto* result_tensor_data = GetTensorData<ValueType>(result_tensor);

    int size = result_tensor_shape.FlatSize();
    std::vector<ValueType> result;
    for (int i = 0; i < size; ++i) {
      result.push_back(result_tensor_data[i]);
    }
    return result;
  }

  std::vector<std::string> GetStringLookupResult() {
    auto* result_tensor = interpreter_->tensor(kResultTensorId);
    auto result_tensor_shape = GetTensorShape(result_tensor);

    int size = result_tensor_shape.FlatSize();
    std::vector<std::string> result;
    for (int i = 0; i < size; ++i) {
      auto string_ref = GetString(result_tensor, i);
      result.push_back(std::string(string_ref.str, string_ref.len));
    }
    return result;
  }

  void AddTensors(bool table_two_initialization = false) {
    int first_new_tensor_index;
    if (!table_two_initialization) {
      ASSERT_EQ(interpreter_->AddTensors(7, &first_new_tensor_index),
                kTfLiteOk);
      ASSERT_EQ(interpreter_->SetInputs({kResourceTensorId, kKeyTensorId,
                                         kValueTensorId, kQueryTensorId,
                                         kDefaultValueTensorId}),
                kTfLiteOk);
      ASSERT_EQ(interpreter_->SetOutputs({kResultTensorId, kSizeTensorId}),
                kTfLiteOk);
    } else {
      ASSERT_EQ(interpreter_->AddTensors(14, &first_new_tensor_index),
                kTfLiteOk);
      ASSERT_EQ(
          interpreter_->SetInputs(
              {kResourceTensorId, kKeyTensorId, kValueTensorId, kQueryTensorId,
               kDefaultValueTensorId, kResourceTwoTensorId, kKeyTwoTensorId,
               kValueTwoTensorId, kQueryTwoTensorId, kDefaultValueTwoTensorId}),
          kTfLiteOk);
      ASSERT_EQ(
          interpreter_->SetOutputs({kResultTensorId, kSizeTensorId,
                                    kResultTwoTensorId, kSizeTwoTensorId}),
          kTfLiteOk);
    }

    // Resource id tensor.
    interpreter_->SetTensorParametersReadWrite(
        kResourceTensorId, kTfLiteResource, "", {}, TfLiteQuantization());

    // Key tensor for import.
    interpreter_->SetTensorParametersReadWrite(kKeyTensorId, key_type_, "",
                                               {static_cast<int>(keys_.size())},
                                               TfLiteQuantization());

    // Value tensor for import.
    interpreter_->SetTensorParametersReadWrite(
        kValueTensorId, value_type_, "", {static_cast<int>(values_.size())},
        TfLiteQuantization());

    // Query tensor for lookup.
    interpreter_->SetTensorParametersReadWrite(
        kQueryTensorId, key_type_, "", {static_cast<int>(queries_.size())},
        TfLiteQuantization());

    // Result tensor for lookup result.
    interpreter_->SetTensorParametersReadWrite(
        kResultTensorId, value_type_, "", {static_cast<int>(queries_.size())},
        TfLiteQuantization());

    // Result tensor for size calculation.
    interpreter_->SetTensorParametersReadWrite(kSizeTensorId, kTfLiteInt64, "",
                                               {1}, TfLiteQuantization());

    // Default value tensor for lookup.
    interpreter_->SetTensorParametersReadWrite(
        kDefaultValueTensorId, value_type_, "", {1}, TfLiteQuantization());

    if (table_two_initialization) {
      // Resource id tensor.
      interpreter_->SetTensorParametersReadWrite(
          kResourceTwoTensorId, kTfLiteResource, "", {}, TfLiteQuantization());

      // Key tensor for import.
      interpreter_->SetTensorParametersReadWrite(
          kKeyTwoTensorId, key_type_, "", {static_cast<int>(keys_two_.size())},
          TfLiteQuantization());

      // Value tensor for import.
      interpreter_->SetTensorParametersReadWrite(
          kValueTwoTensorId, value_type_, "",
          {static_cast<int>(values_two_.size())}, TfLiteQuantization());

      // Query tensor for lookup.
      interpreter_->SetTensorParametersReadWrite(
          kQueryTwoTensorId, key_type_, "",
          {static_cast<int>(queries_two_.size())}, TfLiteQuantization());

      // Result tensor for lookup result.
      interpreter_->SetTensorParametersReadWrite(
          kResultTwoTensorId, value_type_, "",
          {static_cast<int>(queries_two_.size())}, TfLiteQuantization());

      // Result tensor for size calculation.
      interpreter_->SetTensorParametersReadWrite(kSizeTwoTensorId, kTfLiteInt64,
                                                 "", {1}, TfLiteQuantization());

      // Default value tensor for lookup.
      interpreter_->SetTensorParametersReadWrite(
          kDefaultValueTwoTensorId, value_type_, "", {1}, TfLiteQuantization());
    }
  }

  TfLiteStatus AllocateTensors(bool table_two_initialization = false) {
    if (interpreter_->AllocateTensors() != kTfLiteOk) {
      return kTfLiteError;
    }

    SetTensorData(interpreter_.get(), kKeyTensorId, keys_);
    SetTensorData(interpreter_.get(), kValueTensorId, values_);
    SetTensorData(interpreter_.get(), kQueryTensorId, queries_);
    SetTensorData(interpreter_.get(), kDefaultValueTensorId,
                  std::vector<ValueType>({default_value_}));

    if (table_two_initialization) {
      SetTensorData(interpreter_.get(), kKeyTwoTensorId, keys_two_);
      SetTensorData(interpreter_.get(), kValueTwoTensorId, values_two_);
      SetTensorData(interpreter_.get(), kQueryTwoTensorId, queries_two_);
      SetTensorData(interpreter_.get(), kDefaultValueTwoTensorId,
                    std::vector<ValueType>({default_value_two_}));
    }
    return kTfLiteOk;
  }

  TestErrorReporter* GetErrorReporter() { return &error_reporter_; }

 private:
  void InitOpRegistrations() {
    hashtable_registration_ = tflite::ops::builtin::Register_HASHTABLE();
    ASSERT_NE(hashtable_registration_, nullptr);

    hashtable_find_registration_ =
        tflite::ops::builtin::Register_HASHTABLE_FIND();
    ASSERT_NE(hashtable_find_registration_, nullptr);

    hashtable_import_registration_ =
        tflite::ops::builtin::Register_HASHTABLE_IMPORT();
    ASSERT_NE(hashtable_import_registration_, nullptr);

    hashtable_size_registration_ =
        tflite::ops::builtin::Register_HASHTABLE_SIZE();
    ASSERT_NE(hashtable_size_registration_, nullptr);
  }

  TfLiteHashtableParams* GetHashtableParams() {
    TfLiteHashtableParams* params = reinterpret_cast<TfLiteHashtableParams*>(
        malloc(sizeof(TfLiteHashtableParams)));
    params->table_id = std::rand();
    params->key_dtype = key_type_;
    params->value_dtype = value_type_;
    return params;
  }

  // Tensor types
  TfLiteType key_type_;
  TfLiteType value_type_;

  // Tensor data
  std::vector<KeyType> keys_;
  std::vector<ValueType> values_;
  std::vector<KeyType> queries_;
  ValueType default_value_;

  // Tensor data for table two.
  std::vector<KeyType> keys_two_;
  std::vector<ValueType> values_two_;
  std::vector<KeyType> queries_two_;
  ValueType default_value_two_;

  // Op registrations.
  TfLiteRegistration* hashtable_registration_;
  TfLiteRegistration* hashtable_find_registration_;
  TfLiteRegistration* hashtable_import_registration_;
  TfLiteRegistration* hashtable_size_registration_;

  // Hashtable params.
  TfLiteHashtableParams* hashtable_params_;
  TfLiteHashtableParams* hashtable_two_params_;

  // Interpreter.
  std::unique_ptr<Interpreter> interpreter_;
  TestErrorReporter error_reporter_;
};

// HashtableDefaultGraphTest tests hash table features on a basic graph, created
// by the HashtableGraph class.
template <typename KeyType, typename ValueType>
class HashtableDefaultGraphTest {
 public:
  HashtableDefaultGraphTest(TfLiteType key_type, TfLiteType value_type,
                            std::initializer_list<KeyType> keys,
                            std::initializer_list<ValueType> values,
                            std::initializer_list<KeyType> queries,
                            ValueType default_value, int table_size,
                            std::initializer_list<ValueType> lookup_result) {
    graph_ = absl::make_unique<HashtableGraph<KeyType, ValueType>>(key_type,
                                                                   value_type);
    graph_->SetTable(keys, values);
    graph_->SetQuery(queries, default_value);
    graph_->AddTensors();
    graph_->BuildDefaultGraph();

    value_type_ = value_type;
    table_size_ = table_size;
    lookup_result_ = std::vector<ValueType>(lookup_result);
  }

  void Invoke() {
    EXPECT_EQ(graph_->AllocateTensors(), kTfLiteOk);
    EXPECT_EQ(graph_->Invoke(), kTfLiteOk);

    EXPECT_THAT(graph_->GetTableSize(), table_size_);
  }

  void InvokeAndVerifyStringResult() {
    Invoke();
    EXPECT_THAT(graph_->GetStringLookupResult(),
                ElementsAreArray(lookup_result_));
  }

  void InvokeAndVerifyIntResult() {
    Invoke();
    EXPECT_THAT(graph_->GetLookupResult(), ElementsAreArray(lookup_result_));
  }

  void InvokeAndVerifyFloatResult() {
    Invoke();
    EXPECT_THAT(graph_->GetLookupResult(),
                ElementsAreArray(ArrayFloatNear(lookup_result_)));
  }

 private:
  std::unique_ptr<HashtableGraph<KeyType, ValueType>> graph_;

  TfLiteType value_type_;
  int table_size_;
  std::vector<ValueType> lookup_result_;
};

TEST(HashtableOpsTest, TestInt64ToStringHashtable) {
  HashtableDefaultGraphTest<std::int64_t, std::string> t(
      kTfLiteInt64, kTfLiteString,
      /*keys=*/{1, 2, 3}, /*values=*/{"a", "b", "c"}, /*queries=*/{2, 3, 4},
      /*default_value=*/"d", /*table_size=*/3,
      /*lookup_result=*/{"b", "c", "d"});
  t.InvokeAndVerifyStringResult();
}

TEST(HashtableOpsTest, TestStringToInt64Hashtable) {
  HashtableDefaultGraphTest<std::string, int64_t> t(
      kTfLiteString, kTfLiteInt64,
      /*keys=*/{"A", "B", "C"}, /*values=*/{4, 5, 6},
      /*queries=*/{"B", "C", "D"},
      /*default_value=*/-1, /*table_size=*/3, /*lookup_result=*/{5, 6, -1});
  t.InvokeAndVerifyIntResult();
}

TEST(HashtableOpsTest, TestNoImport) {
  HashtableGraph<std::string, std::int64_t> graph(kTfLiteString, kTfLiteInt64);
  graph.SetQuery({"1", "2", "3"}, -1);
  graph.AddTensors();
  graph.BuildNoImportGraph();
  EXPECT_EQ(graph.AllocateTensors(), kTfLiteOk);
  EXPECT_EQ(graph.Invoke(), kTfLiteError);
  EXPECT_TRUE(
      absl::StrContains(graph.GetErrorReporter()->error_messages(),
                        "hashtable need to be initialized before using"));
}

TEST(HashtableOpsTest, TestImportTwice) {
  HashtableGraph<std::string, std::int64_t> graph(kTfLiteString, kTfLiteInt64);
  graph.SetTable({"1", "2", "3"}, {4, 5, 6});
  graph.SetQuery({"2", "3", "4"}, -1);
  graph.AddTensors();
  graph.BuildImportTwiceGraph();
  EXPECT_EQ(graph.AllocateTensors(), kTfLiteOk);
  // The invocation of  thesecond import node will be ignored.
  EXPECT_EQ(graph.Invoke(), kTfLiteOk);

  EXPECT_THAT(graph.GetTableSize(), 3);
  EXPECT_THAT(graph.GetLookupResult(), ElementsAreArray({5, 6, -1}));
}

TEST(HashtableOpsTest, TestTwoHashtables) {
  HashtableGraph<std::string, std::int64_t> graph(kTfLiteString, kTfLiteInt64);
  graph.SetTable({"1", "2", "3"}, {4, 5, 6});
  graph.SetQuery({"2", "3", "4"}, -1);
  graph.SetTableTwo({"-1", "-2", "-3"}, {7, 8, 9});
  graph.SetQueryForTableTwo({"-4", "-2", "-3"}, -2);
  graph.AddTensors(/*table_two_initialization=*/true);
  graph.BuildTwoHashtablesGraph();
  EXPECT_EQ(graph.AllocateTensors(/*table_two_initialization=*/true),
            kTfLiteOk);
  EXPECT_EQ(graph.Invoke(), kTfLiteOk);

  EXPECT_THAT(graph.GetTableSize(), 3);
  EXPECT_THAT(graph.GetTableTwoSize(), 3);
  EXPECT_THAT(graph.GetLookupResult(), ElementsAreArray({5, 6, -1}));
  EXPECT_THAT(graph.GetLookupTwoResult(), ElementsAreArray({-2, 8, 9}));
}

TEST(HashtableOpsTest, TestImportDifferentKeyAndValueSize) {
  HashtableGraph<std::string, std::int64_t> graph(kTfLiteString, kTfLiteInt64);
  graph.SetTable({"1", "2", "3"}, {4, 5});
  graph.SetQuery({"2", "3", "4"}, -1);
  graph.AddTensors();
  graph.BuildDefaultGraph();
  EXPECT_EQ(graph.AllocateTensors(), kTfLiteOk);
  EXPECT_EQ(graph.Invoke(), kTfLiteError);
}

// HashtableOpModel creates a model with one single Hashtable op.
class HashtableOpModel : public SingleOpModel {
 public:
  explicit HashtableOpModel(const int table_id, TensorType key_dtype,
                            TensorType value_dtype) {
    output_ = AddOutput(TensorType_RESOURCE);

    SetBuiltinOp(
        BuiltinOperator_HASHTABLE, BuiltinOptions_HashtableOptions,
        CreateHashtableOptions(builder_, table_id, key_dtype, value_dtype)
            .Union());
    BuildInterpreter({});
  }

  int GetOutput() {
    int* int32_ptr =
        reinterpret_cast<int32_t*>(interpreter_->tensor(0)->data.raw);
    return *int32_ptr;
  }
  std::vector<int> GetOutputShape() { return GetTensorShape(output_); }

  resource::ResourceMap& GetResources() {
    return interpreter_->primary_subgraph().resources();
  }

 private:
  int output_;
};

TEST(HashtableOpsTest, TestHashtable) {
  HashtableOpModel m(/*table_id=*/1, TensorType_INT64, TensorType_STRING);
  EXPECT_EQ(m.GetResources().size(), 0);
  m.Invoke();
  auto& resources = m.GetResources();
  EXPECT_EQ(resources.size(), 1);
  int resource_id = m.GetOutput();
  EXPECT_NE(resource_id, 0);
  auto* hashtable = resource::GetHashtableResource(&resources, resource_id);
  EXPECT_TRUE(hashtable != nullptr);
  EXPECT_TRUE(hashtable->GetKeyType() == kTfLiteInt64);
  EXPECT_TRUE(hashtable->GetValueType() == kTfLiteString);
}

template <typename T>
TfLiteTensor CreateTensor(TfLiteType type, const std::vector<T>& vec) {
  TfLiteTensor tensor = {};
  TfLiteIntArray* dims = TfLiteIntArrayCreate(1);
  dims->data[0] = vec.size();
  tensor.dims = dims;
  tensor.name = "";
  tensor.params = {};
  tensor.quantization = {kTfLiteNoQuantization, nullptr};
  tensor.is_variable = false;
  tensor.allocation_type = kTfLiteDynamic;
  tensor.allocation = nullptr;
  tensor.type = type;
  tensor.bytes = sizeof(T) * vec.size();
  T* data = static_cast<T*>(malloc(sizeof(T) * vec.size()));
  for (int i = 0; i < vec.size(); ++i) {
    data[i] = vec[i];
  }
  tensor.data.raw = reinterpret_cast<char*>(data);
  return tensor;
}

template <>
TfLiteTensor CreateTensor(TfLiteType type,
                          const std::vector<std::string>& vec) {
  TfLiteTensor tensor = {};
  TfLiteIntArray* dims = TfLiteIntArrayCreate(1);
  dims->data[0] = vec.size();
  tensor.dims = dims;
  tensor.name = "";
  tensor.params = {};
  tensor.quantization = {kTfLiteNoQuantization, nullptr};
  tensor.is_variable = false;
  tensor.allocation_type = kTfLiteDynamic;
  tensor.allocation = nullptr;
  tensor.type = type;
  DynamicBuffer buf;
  for (std::string str : vec) {
    buf.AddString(str.c_str(), str.size());
  }
  buf.WriteToTensor(&tensor, nullptr);
  return tensor;
}

template <typename KeyType, typename ValueType>
void InitHashtableResource(resource::ResourceMap* resources, int resource_id,
                           TfLiteType key_type, TfLiteType value_type,
                           std::initializer_list<KeyType> keys,
                           std::initializer_list<ValueType> values) {
  resource::CreateHashtableResourceIfNotAvailable(resources, resource_id,
                                                  key_type, value_type);
  auto lookup = resource::GetHashtableResource(resources, resource_id);

  TfLiteContext context;
  TfLiteTensor key_tensor = CreateTensor<KeyType>(key_type, keys);
  TfLiteTensor value_tensor = CreateTensor<ValueType>(value_type, values);
  lookup->Import(&context, &key_tensor, &value_tensor);
  TfLiteTensorFree(&key_tensor);
  TfLiteTensorFree(&value_tensor);
}

// BaseHashtableOpModel is a base class for creating a model with any single
// hashtable op node, which takes a hash table resource as an input.
class BaseHashtableOpModel : public SingleOpModel {
 public:
  BaseHashtableOpModel() {}

  void SetResourceId(int resource_id) {
    auto* tensor = interpreter_->tensor(resource_id_);

    size_t bytesRequired = sizeof(int32_t);
    TfLiteTensorRealloc(bytesRequired, tensor);
    tensor->bytes = bytesRequired;

    TfLiteIntArray* outputSize = TfLiteIntArrayCreate(1);
    outputSize->data[0] = 1;
    if (tensor->dims) TfLiteIntArrayFree(tensor->dims);
    tensor->dims = outputSize;

    int32_t* resource_ptr = reinterpret_cast<int32_t*>(tensor->data.raw);
    resource_ptr[0] = resource_id;
  }

  void CreateHashtableResource(int resource_id) {
    auto key_tensor = interpreter_->tensor(keys_);
    auto value_tensor = interpreter_->tensor(values_);

    auto& resources = GetResources();
    resource::CreateHashtableResourceIfNotAvailable(
        &resources, resource_id, key_tensor->type, value_tensor->type);
  }

  template <typename ValueType>
  std::vector<ValueType> GetOutput() {
    return ExtractVector<ValueType>(output_);
  }

  std::vector<int> GetOutputShape() { return GetTensorShape(output_); }

  resource::ResourceMap& GetResources() {
    return interpreter_->primary_subgraph().resources();
  }

 protected:
  int resource_id_;
  int keys_;
  int values_;
  int output_;

  TensorType key_type_;
  TensorType value_type_;
};

// HashtableFindOpModel creates a model with a HashtableLookup op.
template <typename KeyType, typename ValueType>
class HashtableFindOpModel : public BaseHashtableOpModel {
 public:
  HashtableFindOpModel(const TensorType key_type, const TensorType value_type,
                       int lookup_size) {
    key_type_ = key_type;
    value_type_ = value_type;

    resource_id_ = AddInput({TensorType_RESOURCE, {1}});
    lookup_ = AddInput({key_type, {lookup_size}});
    default_value_ = AddInput({value_type, {1}});

    output_ = AddOutput({value_type, {lookup_size}});

    SetBuiltinOp(BuiltinOperator_HASHTABLE_FIND,
                 BuiltinOptions_HashtableFindOptions,
                 CreateHashtableFindOptions(builder_).Union());
    BuildInterpreter(
        {GetShape(resource_id_), GetShape(lookup_), GetShape(default_value_)});
  }

  void SetLookup(const std::vector<KeyType>& data) {
    PopulateTensor(lookup_, data);
  }

  void SetStringLookup(const std::vector<std::string>& data) {
    PopulateStringTensor(lookup_, data);
  }

  void SetDefaultValue(const std::vector<ValueType>& data) {
    PopulateTensor(default_value_, data);
  }

  void SetStringDefaultValue(const std::vector<std::string>& data) {
    PopulateStringTensor(default_value_, data);
  }

 private:
  int lookup_;
  int default_value_;
};

TEST(HashtableOpsTest, TestHashtableLookupStringToInt64) {
  const int kResourceId = 42;
  HashtableFindOpModel<std::string, std::int64_t> m(TensorType_STRING,
                                                    TensorType_INT64, 3);

  m.SetResourceId(kResourceId);
  m.SetStringLookup({"5", "6", "7"});
  m.SetDefaultValue({4});

  InitHashtableResource<std::string, std::int64_t>(
      &m.GetResources(), kResourceId, kTfLiteString, kTfLiteInt64,
      {"4", "5", "6"}, {1, 2, 3});
  m.Invoke();

  EXPECT_THAT(m.GetOutput<std::int64_t>(), ElementsAreArray({2, 3, 4}));
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3}));
}

TEST(HashtableOpsTest, TestHashtableLookupInt64ToString) {
  const int kResourceId = 42;
  HashtableFindOpModel<std::int64_t, std::string> m(TensorType_INT64,
                                                    TensorType_STRING, 3);

  m.SetResourceId(kResourceId);
  m.SetLookup({5, 6, 7});
  m.SetStringDefaultValue({"4"});

  InitHashtableResource<std::int64_t, std::string>(
      &m.GetResources(), kResourceId, kTfLiteInt64, kTfLiteString, {4, 5, 6},
      {"1", "2", "3"});
  m.Invoke();

  EXPECT_THAT(m.GetOutput<std::string>(), ElementsAreArray({"2", "3", "4"}));
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({3}));
}

// HashtableImportOpModel creates a model with a HashtableImport op.
template <typename KeyType, typename ValueType>
class HashtableImportOpModel : public BaseHashtableOpModel {
 public:
  HashtableImportOpModel(const TensorType key_type, const TensorType value_type,
                         int initdata_size) {
    key_type_ = key_type;
    value_type_ = value_type;

    resource_id_ = AddInput({TensorType_RESOURCE, {1}});
    keys_ = AddInput({key_type, {initdata_size}});
    values_ = AddInput({value_type, {initdata_size}});

    SetBuiltinOp(BuiltinOperator_HASHTABLE_IMPORT,
                 BuiltinOptions_HashtableImportOptions,
                 CreateHashtableImportOptions(builder_).Union());
    BuildInterpreter(
        {GetShape(resource_id_), GetShape(keys_), GetShape(values_)});
  }

  void SetKeys(const std::vector<KeyType>& data) {
    PopulateTensor(keys_, data);
  }

  void SetStringKeys(const std::vector<std::string>& data) {
    PopulateStringTensor(keys_, data);
  }

  void SetValues(const std::vector<ValueType>& data) {
    PopulateTensor(values_, data);
  }

  void SetStringValues(const std::vector<std::string>& data) {
    PopulateStringTensor(values_, data);
  }
};

TEST(HashtableOpsTest, TestHashtableImport) {
  const int kResourceId = 42;
  HashtableImportOpModel<std::int64_t, std::string> m(TensorType_INT64,
                                                      TensorType_STRING, 3);
  EXPECT_EQ(m.GetResources().size(), 0);
  m.SetResourceId(kResourceId);
  m.SetKeys({1, 2, 3});
  m.SetStringValues({"1", "2", "3"});
  m.CreateHashtableResource(kResourceId);
  m.Invoke();

  auto& resources = m.GetResources();
  EXPECT_EQ(resources.size(), 1);
  auto* hashtable = resource::GetHashtableResource(&resources, kResourceId);
  EXPECT_TRUE(hashtable != nullptr);
  EXPECT_TRUE(hashtable->GetKeyType() == kTfLiteInt64);
  EXPECT_TRUE(hashtable->GetValueType() == kTfLiteString);

  EXPECT_EQ(hashtable->Size(), 3);
}

TEST(HashtableOpsTest, TestHashtableImportTwice) {
  const int kResourceId = 42;
  HashtableImportOpModel<std::int64_t, std::string> m(TensorType_INT64,
                                                      TensorType_STRING, 3);
  EXPECT_EQ(m.GetResources().size(), 0);
  m.SetResourceId(kResourceId);
  m.SetKeys({1, 2, 3});
  m.SetStringValues({"1", "2", "3"});
  m.CreateHashtableResource(kResourceId);
  m.Invoke();
  m.Invoke();

  auto& resources = m.GetResources();
  EXPECT_EQ(resources.size(), 1);
  auto* hashtable = resource::GetHashtableResource(&resources, kResourceId);
  EXPECT_TRUE(hashtable != nullptr);
  EXPECT_TRUE(hashtable->GetKeyType() == kTfLiteInt64);
  EXPECT_TRUE(hashtable->GetValueType() == kTfLiteString);
  EXPECT_EQ(hashtable->Size(), 3);
}

// HashtableSizeOpModel creates a model with a HashtableSize op.
template <typename KeyType, typename ValueType>
class HashtableSizeOpModel : public BaseHashtableOpModel {
 public:
  HashtableSizeOpModel(const TensorType key_type, const TensorType value_type) {
    key_type_ = key_type;
    value_type_ = value_type;

    resource_id_ = AddInput({TensorType_RESOURCE, {1}});

    output_ = AddOutput({TensorType_INT64, {1}});

    SetBuiltinOp(BuiltinOperator_HASHTABLE_SIZE,
                 BuiltinOptions_HashtableSizeOptions,
                 CreateHashtableSizeOptions(builder_).Union());
    BuildInterpreter({GetShape(resource_id_)});
  }
};

TEST(HashtableOpsTest, TestHashtableSize) {
  const int kResourceId = 42;
  HashtableSizeOpModel<std::string, std::int64_t> m(TensorType_STRING,
                                                    TensorType_INT64);

  m.SetResourceId(kResourceId);

  InitHashtableResource<std::string, std::int64_t>(
      &m.GetResources(), kResourceId, kTfLiteString, kTfLiteInt64,
      {"4", "5", "6"}, {1, 2, 3});
  m.Invoke();

  EXPECT_THAT(m.GetOutput<std::int64_t>(), ElementsAreArray({3}));
  EXPECT_THAT(m.GetOutputShape(), ElementsAreArray({1}));
}

TEST(HashtableOpsTest, TestHashtableSizeNonInitialized) {
  const int kResourceId = 42;
  HashtableSizeOpModel<std::string, std::int64_t> m(TensorType_STRING,
                                                    TensorType_INT64);
  m.SetResourceId(kResourceId);

  // Invoke without hash table initialization.
  EXPECT_NE(m.InvokeUnchecked(), kTfLiteOk);
}

}  // namespace
}  // namespace tflite
