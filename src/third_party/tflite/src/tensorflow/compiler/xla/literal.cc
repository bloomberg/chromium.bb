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

#include "tensorflow/compiler/xla/literal.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <numeric>
#include <type_traits>
#include <vector>

#include "absl/base/casts.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "tensorflow/compiler/xla/index_util.h"
#include "tensorflow/compiler/xla/permutation_util.h"
#include "tensorflow/compiler/xla/primitive_util.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/hash/hash.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/mem.h"
#include "tensorflow/core/platform/types.h"

namespace xla {
namespace {

using absl::StrCat;

constexpr bool kLittleEndian = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
// Literals can be used as DMA targets, which can require alignment. We
// force a tensorflow::Allocator::kAllocatorAlignment-byte minimum
// alignment.
constexpr int kMinimumAlignment = 64;

// Converts between little and big endian.
//
// Precondition: size % 2 == 0 (elements in the array are 16 bits long)
void ConvertEndianShort(string* bytes) {
  CHECK_EQ(bytes->size() % 2, 0);
  for (int64_t i = 0, end = bytes->size(); i < end; i += 2) {
    std::swap((*bytes)[i], (*bytes)[i + 1]);
  }
}

void ConvertEndianShort(char* bytes, int64_t size) {
  CHECK_EQ(size % 2, 0);
  for (int64_t i = 0; i < size; i += 2) {
    std::swap(bytes[i], bytes[i + 1]);
  }
}

string CompactOneline(const string& input) {
  string result;
  std::vector<string> v = absl::StrSplit(input, absl::ByAnyChar("\n "));
  bool first = true;
  // Concatenate elements in "v" with spaces separating them, but ignoring
  // empty entries.
  for (const auto& s : v) {
    if (s.empty()) {
      continue;
    }
    absl::StrAppend(&result, (first ? "" : " "), s);
    first = false;
  }
  return result;
}

// Since Eigen::half doesn't satisfy the absl::bit_cast contract, we need to be
// able to transparently access the raw 16-bit value contained within.
template <typename T>
T GetRawValue(T val) {
  return val;
}
uint16 GetRawValue(Eigen::half val) {
  return Eigen::numext::bit_cast<uint16>(val);
}

bool LiteralProtoHasValues(const LiteralProto& proto) {
  return proto.preds_size() || !proto.s8s().empty() || !proto.u8s().empty() ||
         proto.s32s_size() || proto.s64s_size() || proto.u32s_size() ||
         proto.u64s_size() || proto.f32s_size() || proto.f64s_size() ||
         proto.c64s_size() || proto.c128s_size() ||
         proto.tuple_literals_size() || !proto.f16s().empty() ||
         !proto.bf16s().empty() || !proto.u16s().empty() ||
         !proto.s16s().empty();
}

}  // namespace

LiteralBase::~LiteralBase() {}

std::ostream& operator<<(std::ostream& out, const Literal& literal) {
  out << literal.ToString();
  return out;
}

MutableLiteralBase::StrideConfig::StrideConfig(
    const Shape& source_shape, const Shape& dest_shape,
    absl::Span<const int64_t> dimensions)
    : dimensions(dimensions),
      base(dimensions.size(), 0),
      step(dimensions.size(), 1) {
  if (!dimensions.empty()) {
    // Selects the shape with the largest minor dimension as the one upon
    // which to run the tight stride loop.
    if (dimensions[LayoutUtil::Minor(source_shape.layout(), 0)] >=
        dimensions[LayoutUtil::Minor(dest_shape.layout(), 0)]) {
      minor_dimension = LayoutUtil::Minor(source_shape.layout(), 0);
      dest_stride = IndexUtil::GetDimensionStride(dest_shape, minor_dimension);
    } else {
      minor_dimension = LayoutUtil::Minor(dest_shape.layout(), 0);
      source_stride =
          IndexUtil::GetDimensionStride(source_shape, minor_dimension);
    }
    minor_loop_size = dimensions[minor_dimension];
    step[minor_dimension] = minor_loop_size;
  }
}

Literal::Literal(const Shape& shape)
    : Literal(shape, /*allocate_arrays=*/true) {}

void Literal::SetPiece(const Shape& shape, Piece* piece, bool allocate_arrays) {
  if (shape.IsTuple()) {
    for (int i = 0; i < ShapeUtil::TupleElementCount(shape); ++i) {
      const Shape& subshape = shape.tuple_shapes(i);

      auto child_piece = Piece();
      child_piece.set_subshape(&subshape);

      SetPiece(subshape, &child_piece, allocate_arrays);

      piece->emplace_back(std::move(child_piece));
    }
  } else if (shape.IsArray()) {
    if (allocate_arrays) {
      piece->set_buffer(static_cast<char*>(tensorflow::port::AlignedMalloc(
          piece->size_bytes(), kMinimumAlignment)));
      if (shape.is_dynamic()) {
        CHECK_EQ(piece->dynamic_size_buffer(), nullptr);
        piece->set_dynamic_size_buffer(
            static_cast<int32*>(tensorflow::port::AlignedMalloc(
                piece->dynamic_size_buffer_bytes(), kMinimumAlignment)));
      }
    }
  } else {
    // If the shape is neither an array nor tuple, then it must be
    // zero-sized. Otherwise, some memory needs to be allocated for it.
    CHECK_EQ(piece->size_bytes(), 0);
  }
}

Literal::Literal(const Shape& shape, bool allocate_arrays)
    : MutableLiteralBase() {
  shape_ = absl::make_unique<Shape>(shape);
  CHECK(LayoutUtil::HasLayout(*shape_));
  root_piece_ = new Piece();
  root_piece_->set_subshape(shape_.get());
  CHECK(&root_piece_->subshape() == shape_.get());

  SetPiece(*shape_, root_piece_, allocate_arrays);
}

Literal::~Literal() {
  if (root_piece_ != nullptr) {
    DeallocateBuffers();
    delete root_piece_;
  }
}

void Literal::DeallocateBuffers() {
  root_piece_->ForEachMutableSubpiece(
      [&](const ShapeIndex& index, Piece* piece) {
        if (piece->buffer() != nullptr) {
          tensorflow::port::AlignedFree(piece->buffer());
        }
        if (piece->dynamic_size_buffer() != nullptr) {
          tensorflow::port::AlignedFree(piece->dynamic_size_buffer());
        }
      });
}

Literal::Literal(Literal&& other) : MutableLiteralBase() {
  *this = std::move(other);
}

Literal& Literal::operator=(Literal&& other) {
  DCHECK(&other.root_piece_->subshape() == other.shape_.get());
  using std::swap;
  swap(shape_, other.shape_);
  swap(root_piece_, other.root_piece_);
  DCHECK(&root_piece_->subshape() == shape_.get());

  return *this;
}

Literal LiteralBase::CreateFromShape(const Shape& shape) {
  Literal literal(shape);
  literal.root_piece_->ForEachMutableSubpiece(
      [&](const ShapeIndex& index, Piece* piece) {
        if (piece->subshape().IsArray()) {
          memset(piece->untyped_data(), 0, piece->size_bytes());
        }
      });
  return literal;
}

int32 LiteralBase::GetDynamicSize(int64_t dim_index) const {
  return GetDynamicSize(dim_index, {});
}

int32 LiteralBase::GetDynamicSize(int64_t dim_index,
                                  const ShapeIndex& shape_index) const {
  return piece(shape_index).GetDynamicSize(dim_index);
}

absl::optional<int64_t> LiteralBase::GetFirstInteger() const {
  switch (shape().element_type()) {
    case U8:
      return GetFirstElement<uint8>();
    case U16:
      return GetFirstElement<uint16>();
    case U32:
      return GetFirstElement<uint32>();
    case U64: {
      int64_t v = GetFirstElement<uint64_t>();
      if (v < 0) {
        return absl::nullopt;
      }
      return v;
    }
    case S8:
      return GetFirstElement<int8>();
    case S16:
      return GetFirstElement<int16>();
    case S32:
      return GetFirstElement<int32>();
    case S64:
      return GetFirstElement<int64_t>();
    default:
      return absl::nullopt;
  }
}

template <typename NativeT>
Status MutableLiteralBase::CopySliceFromInternal(
    const LiteralBase& src_literal, absl::Span<const int64_t> src_base,
    absl::Span<const int64_t> dest_base, absl::Span<const int64_t> copy_size) {
  const int64_t src_base_size = src_base.size();
  const int64_t dest_base_size = dest_base.size();
  TF_RET_CHECK(src_literal.shape().rank() == src_base_size);
  TF_RET_CHECK(shape().rank() == dest_base_size);

  auto linear_index = [](const Shape& shape,
                         absl::Span<const int64_t> multi_index) {
    return IndexUtil::MultidimensionalIndexToLinearIndex(shape, multi_index);
  };

  if (src_literal.shape().rank() == 0 || shape().rank() == 0) {
    // If any of the two shapes are scalars, we can just call the StridedCopy()
    // directly, and we know we will be copying only one value.
    TF_RET_CHECK(copy_size.empty());
    StridedCopy(data<NativeT>(), linear_index(shape(), dest_base), 0,
                src_literal.data<NativeT>(),
                linear_index(src_literal.shape(), src_base), 0, 1);
  } else if (!ShapeUtil::IsZeroElementArray(shape()) &&
             !ShapeUtil::IsZeroElementArray(src_literal.shape())) {
    // Perform copy if neither src nor dest has dimensions with zero element,
    // otherwise it's a no-op.
    TF_RET_CHECK(src_base.size() == dest_base.size());
    TF_RET_CHECK(src_base.size() == copy_size.size());

    // Scan the source from minor, stepping in copy size blocks, then within
    // the index enumeration functor, do a strided copy advancing source index
    // by one (walking through the minor dimension), and destination index by
    // proper stride size at the matching dimension.
    DimensionVector src_indexes(src_base.size(), 0);
    DimensionVector dest_indexes(dest_base.size(), 0);
    MutableLiteralBase::StrideConfig stride_config(src_literal.shape(), shape(),
                                                   copy_size);

    auto copy_proc = [&](absl::Span<const int64_t> indexes) {
      // Map from multi-dimensional index, to source index.
      std::transform(indexes.begin(), indexes.end(), src_base.begin(),
                     src_indexes.begin(), std::plus<int64_t>());
      // Map from multi-dimensional index, to destination index.
      std::transform(indexes.begin(), indexes.end(), dest_base.begin(),
                     dest_indexes.begin(), std::plus<int64_t>());

      int64_t src_index = linear_index(src_literal.shape(), src_indexes);
      int64_t dest_index = linear_index(shape(), dest_indexes);

      // `this->` is needed to workaround MSVC bug: #16882
      StridedCopy(this->data<NativeT>(), dest_index, stride_config.dest_stride,
                  src_literal.data<NativeT>(), src_index,
                  stride_config.source_stride, stride_config.minor_loop_size);
      return true;
    };

    ShapeUtil::ForEachIndex(src_literal.shape(), stride_config.base,
                            stride_config.dimensions, stride_config.step,
                            copy_proc);
  }
  return Status::OK();
}

Status MutableLiteralBase::CopyElementFrom(
    const LiteralSlice& src_literal, absl::Span<const int64_t> src_index,
    absl::Span<const int64_t> dest_index) {
  DCHECK_EQ(shape().element_type(), src_literal.shape().element_type());
  const int64_t src_linear_index =
      IndexUtil::MultidimensionalIndexToLinearIndex(src_literal.shape(),
                                                    src_index);
  const int64_t dest_linear_index =
      IndexUtil::MultidimensionalIndexToLinearIndex(shape(), dest_index);
  const int64_t primitive_size =
      ShapeUtil::ByteSizeOfPrimitiveType(shape().element_type());

  char* dest_address =
      static_cast<char*>(untyped_data()) + dest_linear_index * primitive_size;
  const char* source_address =
      static_cast<const char*>(src_literal.untyped_data()) +
      src_linear_index * primitive_size;
  if (dest_address != source_address) {
    memcpy(dest_address, source_address, primitive_size);
  }
  return Status::OK();
}

/* static */ StatusOr<Literal> MutableLiteralBase::CreateFromProto(
    const LiteralProto& proto, bool prohibit_empty_literal) {
  if (!proto.has_shape()) {
    return InvalidArgument("LiteralProto has no shape");
  }
  Shape shape(proto.shape());
  if (ShapeUtil::HasPrimitiveType(shape, OPAQUE_TYPE)) {
    return InvalidArgument(
        "Literal shape cannot include OPAQUE_TYPE sub-shape");
  }
  if (!LayoutUtil::HasLayout(shape)) {
    return InvalidArgument("LiteralProto has no layout");
  }

  TF_RETURN_IF_ERROR(ShapeUtil::ValidateShapeWithOptionalLayout(shape));

  Literal literal(shape);

  TF_RETURN_IF_ERROR(literal.root_piece_->ForEachMutableSubpieceWithStatus(
      [&](const ShapeIndex& index, Piece* piece) {
        const LiteralProto* proto_element = &proto;
        for (int64_t i : index) {
          CHECK(i < proto_element->tuple_literals_size());
          proto_element = &proto_element->tuple_literals(i);
        }

        if (piece->subshape().IsTuple()) {
          if (proto_element->tuple_literals_size() !=
              ShapeUtil::TupleElementCount(piece->subshape())) {
            return InvalidArgument(
                "Expected %d tuple elements in LiteralProto, has %d",
                ShapeUtil::TupleElementCount(piece->subshape()),
                proto_element->tuple_literals_size());
          }
          return Status::OK();
        }
        if (piece->subshape().element_type() == TOKEN) {
          return Status::OK();
        }

        CHECK(piece->subshape().IsArray());

        // When prohibit_empty_literal is false (allowing literal with no
        // values), only copy from proto if the literal proto has values. This
        // mode is used for a learned cost model.
        if (prohibit_empty_literal || LiteralProtoHasValues(*proto_element)) {
          TF_RETURN_IF_ERROR(piece->CopyFromProto(*proto_element));
        }

        return Status::OK();
      }));

  return std::move(literal);
}

Literal Literal::SubLiteral(ShapeIndexView shape_index) {
  if (!shape_index.empty()) {
    auto decomposed = this->DecomposeTuple();
    return decomposed.at(shape_index.front())
        .SubLiteral(shape_index.ConsumeFront());
  } else {
    return std::move(*this);
  }
}

std::vector<Literal> Literal::DecomposeTuple() {
  CHECK(shape().IsTuple());
  std::vector<Literal> elements;
  for (int i = 0; i < ShapeUtil::TupleElementCount(shape()); ++i) {
    elements.push_back(Literal(ShapeUtil::GetSubshape(shape(), {i}),
                               /*allocate_arrays=*/false));
    Literal& element = elements.back();
    element.root_piece_->ForEachMutableSubpiece(
        [&](const ShapeIndex& index, Piece* dest_piece) {
          ShapeIndex src_index = {i};
          for (int64_t j : index) {
            src_index.push_back(j);
          }
          Piece& src_piece = piece(src_index);

          // Move the respective buffer over to the element Literal.
          dest_piece->set_buffer(src_piece.buffer());
          dest_piece->set_dynamic_size_buffer(src_piece.dynamic_size_buffer());
          src_piece.set_buffer(nullptr);
          src_piece.set_dynamic_size_buffer(nullptr);
        });
  }
  // Set this literal to be nil-shaped.
  *this = Literal();
  return elements;
}

namespace {

// Copies the elements in 'src' to 'dest'. The shape and layout of the data in
// the array slices are indicated by dest_shape and src_shape respectively.
template <typename NativeT>
void CopyElementsBetween(absl::Span<NativeT> dest,
                         absl::Span<const NativeT> src, const Shape& dest_shape,
                         const Shape& src_shape) {
  CHECK(ShapeUtil::Compatible(dest_shape, src_shape));
  if (ShapeUtil::IsZeroElementArray(dest_shape)) {
    return;
  }
  std::vector<int64_t> index(dest_shape.rank());
  do {
    dest[IndexUtil::MultidimensionalIndexToLinearIndex(dest_shape, index)] =
        src[IndexUtil::MultidimensionalIndexToLinearIndex(src_shape, index)];
  } while (IndexUtil::BumpIndices(dest_shape, absl::MakeSpan(index)));
}
}  // namespace

int32 LiteralBase::Piece::GetDynamicSize(int64_t dim_index) const {
  CHECK(LayoutUtil::IsDenseArray(subshape()));
  if (!subshape_->is_dynamic_dimension(dim_index)) {
    // This is a static dimension, return size.
    return subshape_->dimensions(dim_index);
  }
  CHECK_NE(dynamic_size_buffer(), nullptr);
  return dynamic_size_buffer_[dim_index];
}

void LiteralBase::Piece::SetDynamicSize(int64_t dim_index, int32_t size) {
  CHECK(LayoutUtil::IsDenseArray(subshape()));
  CHECK(subshape_->is_dynamic_dimension(dim_index));
  if (dynamic_size_buffer() == nullptr) {
    // Lazily initialize the dynamic size buffer.
    set_dynamic_size_buffer(static_cast<int32*>(tensorflow::port::AlignedMalloc(
        dynamic_size_buffer_bytes(), kMinimumAlignment)));
    /*for (int64_t i = 0; i < subshape().rank(); ++i) {
      // Initialized to -1 to help debug.
      dynamic_size_buffer_[i] = -1;
    }*/
  }
  dynamic_size_buffer_[dim_index] = size;
}

Status LiteralBase::Piece::CopyFrom(const LiteralBase::Piece& src,
                                    bool only_dynamic_bound) {
  CHECK(subshape_ != nullptr);
  CHECK(src.subshape_ != nullptr);
  if (ShapeUtil::Equal(subshape(), src.subshape())) {
    // If the layouts are equal it's faster just to memcpy.
    memcpy(buffer(), src.buffer(), src.size_bytes());
  } else {
    std::vector<int64_t> origin(subshape().rank(), 0);
    switch (subshape().element_type()) {
#define COPY_ELEMENTS(XLA_T, NATIVE_T)                                      \
  case (XLA_T):                                                             \
    if (only_dynamic_bound) {                                               \
      CopyElementsWithDynamicBound<NATIVE_T>(src);                          \
    } else {                                                                \
      CopyElementsBetween<NATIVE_T>(data<NATIVE_T>(), src.data<NATIVE_T>(), \
                                    subshape(), src.subshape());            \
    }                                                                       \
    break;
      COPY_ELEMENTS(U8, uint8);
      COPY_ELEMENTS(U16, uint16);
      COPY_ELEMENTS(U32, uint32);
      COPY_ELEMENTS(U64, uint64);
      COPY_ELEMENTS(S8, int8);
      COPY_ELEMENTS(S16, int16);
      COPY_ELEMENTS(S32, int32);
      COPY_ELEMENTS(S64, int64_t);
      COPY_ELEMENTS(F16, half);
      COPY_ELEMENTS(BF16, bfloat16);
      COPY_ELEMENTS(F32, float);
      COPY_ELEMENTS(F64, double);
      COPY_ELEMENTS(C64, complex64);
      COPY_ELEMENTS(C128, complex128);
      COPY_ELEMENTS(PRED, bool);
#undef COPY_ELEMENTS
      default:
        return Unimplemented(
            "Copying a Literal object with element type %s is not implemented.",
            PrimitiveType_Name(subshape().element_type()));
    }
  }
  DCHECK_EQ(dynamic_size_buffer_bytes(), src.dynamic_size_buffer_bytes());
  if (subshape().is_dynamic() && src.subshape().is_dynamic()) {
    CHECK_NE(dynamic_size_buffer_, nullptr);
    CHECK_NE(src.dynamic_size_buffer_, nullptr);
    memcpy(dynamic_size_buffer(), src.dynamic_size_buffer(),
           src.dynamic_size_buffer_bytes());
  }
  return Status::OK();
}

void MutableLiteralBase::SetDynamicSize(int64_t dim_index, int32_t size) {
  return SetDynamicSize(dim_index, {}, size);
}

void MutableLiteralBase::SetDynamicSize(int64_t dim_index,
                                        const ShapeIndex& shape_index,
                                        int32_t size) {
  Shape* subshape_ = ShapeUtil::GetMutableSubshape(shape_.get(), shape_index);
  CHECK_GE(subshape_->dimensions(dim_index), size);
  if (subshape_->dimensions(dim_index) == size) {
    subshape_->set_dynamic_dimension(dim_index, false);
    return;
  }
  subshape_->set_dynamic_dimension(dim_index, true);
  piece(shape_index).SetDynamicSize(dim_index, size);
}

Status MutableLiteralBase::CopyFrom(const LiteralSlice& src_literal,
                                    const ShapeIndex& dest_shape_index,
                                    const ShapeIndex& src_shape_index,
                                    bool only_dynamic_bound) {
  const Shape& dest_subshape =
      ShapeUtil::GetSubshape(shape(), dest_shape_index);
  const Shape& src_subshape =
      ShapeUtil::GetSubshape(src_literal.shape(), src_shape_index);
  if (only_dynamic_bound) {
    auto bound_shape = dest_subshape.is_static() ? src_subshape : dest_subshape;
    auto compact_shape =
        dest_subshape.is_static() ? dest_subshape : src_subshape;
    CHECK(ShapeUtil::DynamicShapeIsCompatible(compact_shape, bound_shape))
        << compact_shape.ToString() << " vs " << bound_shape.ToString();
  } else {
    if (!ShapeUtil::Compatible(dest_subshape, src_subshape)) {
      return InvalidArgument(
          "Destination subshape incompatible with source subshape: %s vs %s",
          ShapeUtil::HumanString(dest_subshape),
          ShapeUtil::HumanString(src_subshape));
    }
  }
  return root_piece_->ForEachMutableSubpieceWithStatus(
      [&](const ShapeIndex& index, Piece* piece) {
        if (!piece->subshape().IsArray()) {
          return Status::OK();
        }

        // Determine if this index is in the part of this literal that we want
        // to copy over from src_literal.
        bool in_subtree_to_copy = true;
        for (int i = 0; i < dest_shape_index.size(); ++i) {
          if (index[i] != dest_shape_index[i]) {
            in_subtree_to_copy = false;
            break;
          }
        }
        if (!in_subtree_to_copy) {
          return Status::OK();
        }
        // Construct the index of the corresponding piece in the source literal.
        ShapeIndex src_piece_index = src_shape_index;
        for (int64_t i = dest_shape_index.size(), end = index.size(); i < end;
             ++i) {
          src_piece_index.push_back(index[i]);
        }
        TF_RETURN_IF_ERROR(
            piece->CopyFrom(src_literal.piece(src_piece_index),
                            /*only_dynamic_bound=*/only_dynamic_bound));
        return Status::OK();
      });
}

Status Literal::MoveFrom(Literal&& src_literal,
                         const ShapeIndex& dest_shape_index) {
  const Shape& dest_subshape =
      ShapeUtil::GetSubshape(shape(), dest_shape_index);
  if (!ShapeUtil::Equal(dest_subshape, src_literal.shape())) {
    return InvalidArgument(
        "Destination subshape not equal to source shape: %s vs %s",
        ShapeUtil::HumanString(dest_subshape),
        ShapeUtil::HumanString(src_literal.shape()));
  }

  src_literal.root_piece_->ForEachSubpiece(
      [&](const ShapeIndex& src_index, const Piece& src_piece) {
        if (!src_piece.subshape().IsArray()) {
          return;
        }

        ShapeIndex dest_index = dest_shape_index;
        for (int64_t i : src_index) {
          dest_index.push_back(i);
        }
        Piece& dest_piece = piece(dest_index);
        tensorflow::port::AlignedFree(dest_piece.buffer());
        tensorflow::port::AlignedFree(dest_piece.dynamic_size_buffer());
        dest_piece.set_buffer(src_piece.buffer());
        dest_piece.set_dynamic_size_buffer(src_piece.dynamic_size_buffer());
      });

  src_literal.shape_ = absl::make_unique<Shape>(ShapeUtil::MakeNil());
  delete src_literal.root_piece_;
  src_literal.root_piece_ = new LiteralBase::Piece();
  src_literal.root_piece_->set_subshape(src_literal.shape_.get());

  return Status::OK();
}

Status MutableLiteralBase::CopySliceFrom(const LiteralSlice& src_literal,
                                         absl::Span<const int64_t> src_base,
                                         absl::Span<const int64_t> dest_base,
                                         absl::Span<const int64_t> copy_size) {
  TF_RET_CHECK(shape().IsArray()) << ShapeUtil::HumanString(shape());
  TF_RET_CHECK(src_literal.shape().IsArray())
      << ShapeUtil::HumanString(src_literal.shape());
  TF_RET_CHECK(ShapeUtil::SameElementType(src_literal.shape(), shape()));

  switch (shape().element_type()) {
    case U8:
      return CopySliceFromInternal<uint8>(src_literal, src_base, dest_base,
                                          copy_size);
    case U16:
      return CopySliceFromInternal<uint16>(src_literal, src_base, dest_base,
                                           copy_size);
    case U32:
      return CopySliceFromInternal<uint32>(src_literal, src_base, dest_base,
                                           copy_size);
    case U64:
      return CopySliceFromInternal<uint64_t>(src_literal, src_base, dest_base,
                                             copy_size);
    case S8:
      return CopySliceFromInternal<int8>(src_literal, src_base, dest_base,
                                         copy_size);
    case S16:
      return CopySliceFromInternal<int16>(src_literal, src_base, dest_base,
                                          copy_size);
    case S32:
      return CopySliceFromInternal<int32>(src_literal, src_base, dest_base,
                                          copy_size);
    case S64:
      return CopySliceFromInternal<int64_t>(src_literal, src_base, dest_base,
                                            copy_size);
    case F16:
      return CopySliceFromInternal<half>(src_literal, src_base, dest_base,
                                         copy_size);
    case BF16:
      return CopySliceFromInternal<bfloat16>(src_literal, src_base, dest_base,
                                             copy_size);
    case F32:
      return CopySliceFromInternal<float>(src_literal, src_base, dest_base,
                                          copy_size);
    case F64:
      return CopySliceFromInternal<double>(src_literal, src_base, dest_base,
                                           copy_size);
    case C64:
      return CopySliceFromInternal<complex64>(src_literal, src_base, dest_base,
                                              copy_size);
    case C128:
      return CopySliceFromInternal<complex128>(src_literal, src_base, dest_base,
                                               copy_size);
    case PRED:
      return CopySliceFromInternal<bool>(src_literal, src_base, dest_base,
                                         copy_size);
    default:
      break;
  }
  return Unimplemented(
      "Copying a slice from a Literal object with element type %d is not "
      "implemented.",
      shape().element_type());
}

void MutableLiteralBase::PopulateR1(const tensorflow::core::Bitmap& values) {
  CHECK(shape().IsArray());
  CHECK_EQ(shape().rank(), 1);
  CHECK_EQ(element_count(), values.bits());
  CHECK_EQ(shape().element_type(), PRED);
  for (int64_t i = 0; i < static_cast<int64_t>(values.bits()); ++i) {
    Set({i}, values.get(i));
  }
}

Literal LiteralBase::Relayout(const Layout& new_layout,
                              const ShapeIndex& shape_index) const {
  // Create new shape with 'new_layout' set at the given shape index.
  Shape new_shape = shape();
  Shape* subshape = ShapeUtil::GetMutableSubshape(&new_shape, shape_index);
  TF_CHECK_OK(LayoutUtil::ValidateLayoutForShape(new_layout, *subshape));
  *subshape->mutable_layout() = new_layout;
  Literal result(new_shape);
  TF_CHECK_OK(result.CopyFrom(*this));
  return result;
}

Literal LiteralBase::Relayout(const Shape& shape_with_layout) const {
  CHECK(ShapeUtil::Compatible(shape_with_layout, shape()))
      << "Given shape_with_layout " << ShapeUtil::HumanString(shape_with_layout)
      << " not compatible with literal shape "
      << ShapeUtil::HumanString(shape());
  Literal result = CreateFromShape(shape_with_layout);
  ShapeUtil::ForEachSubshape(
      result.shape(),
      [this, &result](const Shape& subshape, const ShapeIndex& index) {
        if (subshape.IsArray()) {
          TF_CHECK_OK(result.CopyFrom(*this,
                                      /*dest_shape_index=*/index,
                                      /*src_shape_index=*/index));
        }
      });
  return result;
}

Literal LiteralBase::ToBoundedDynamic(const Shape& bounded_shape) const {
  CHECK(bounded_shape.is_dynamic());
  Literal result(bounded_shape);
  ShapeUtil::ForEachSubshape(
      shape(), [&](const Shape& subshape, const ShapeIndex& index) {
        if (!subshape.IsArray()) {
          return;
        }
        for (int64_t i = 0; i < subshape.rank(); ++i) {
          result.SetDynamicSize(i, subshape.dimensions(i));
        }
      });
  TF_CHECK_OK(result.CopyFrom(*this, {}, {}, /*only_dynamic_bound=*/true));

  return result;
}

Literal LiteralBase::ToStatic() const {
  // Create new shape with 'new_layout' set at the given shape index.
  Shape new_shape = shape();
  ShapeUtil::ForEachMutableSubshape(
      &new_shape, [this](Shape* subshape, const ShapeIndex& index) {
        if (!subshape->IsArray()) {
          return;
        }
        for (int64_t i = 0; i < subshape->rank(); ++i) {
          subshape->set_dynamic_dimension(i, false);
          subshape->set_dimensions(i, GetDynamicSize(i, index));
        }
      });
  Literal result(new_shape);
  TF_CHECK_OK(result.CopyFrom(*this, {}, {}, /*only_dynamic_bound=*/true));
  return result;
}

StatusOr<Literal> LiteralBase::Broadcast(
    const Shape& result_shape, absl::Span<const int64_t> dimensions) const {
  if (!shape().IsArray()) {
    return InvalidArgument("Broadcast only supports arrays.");
  }

  for (int64_t i = 0, end = dimensions.size(); i < end; i++) {
    TF_RET_CHECK(shape().dimensions(i) ==
                 result_shape.dimensions(dimensions[i]));
  }

  Literal result(result_shape);

  // scratch_source_index is temporary storage space for the computed index into
  // the input literal.  We put it here to avoid allocating an std::vector in
  // every iteration of ShapeUtil::ForEachIndex.
  std::vector<int64_t> scratch_source_index(shape().dimensions_size());

  char* dest_data = static_cast<char*>(result.untyped_data());
  const char* source_data = static_cast<const char*>(untyped_data());
  const int64_t primitive_size =
      ShapeUtil::ByteSizeOfPrimitiveType(shape().element_type());

  for (int64_t i = 0; i < dimensions.size(); ++i) {
    int64_t dynamic_size = GetDynamicSize(i);
    result.SetDynamicSize(dimensions[i], dynamic_size);
  }

  ShapeUtil::ForEachIndex(
      result_shape, [&](absl::Span<const int64_t> output_index) {
        for (int64_t i = 0, end = dimensions.size(); i < end; ++i) {
          scratch_source_index[i] = output_index[dimensions[i]];
        }
        int64_t dest_index = IndexUtil::MultidimensionalIndexToLinearIndex(
            result_shape, output_index);
        int64_t source_index = IndexUtil::MultidimensionalIndexToLinearIndex(
            shape(), scratch_source_index);
        memcpy(dest_data + primitive_size * dest_index,
               source_data + primitive_size * source_index, primitive_size);
        return true;
      });

  return std::move(result);
}

StatusOr<Literal> LiteralBase::Reshape(
    absl::Span<const int64_t> dimensions) const {
  if (!shape().IsArray()) {
    return InvalidArgument("Reshape does not support tuples.");
  }
  if (shape().is_dynamic()) {
    return Unimplemented("Dynamic reshape is not implemented.");
  }
  Literal output;
  if (!LayoutUtil::IsMonotonicWithDim0Major(shape().layout())) {
    output = Relayout(LayoutUtil::GetDefaultLayoutForRank(shape().rank()));
  } else {
    output = Clone();
  }
  // Because the layout is monotonic, we can simply reuse the same sequence of
  // values without changing their order.
  *output.mutable_shape_do_not_use() =
      ShapeUtil::MakeShape(shape().element_type(), dimensions);

  int64_t elements_before = ShapeUtil::ElementsIn(shape());
  int64_t elements_after = ShapeUtil::ElementsIn(output.shape());
  if (elements_before != elements_after) {
    return InvalidArgument(
        "Shapes before and after Literal::Reshape have different numbers "
        "of elements: %s vs %s.",
        ShapeUtil::HumanString(shape()),
        ShapeUtil::HumanString(output.shape()));
  }
  return std::move(output);
}

Literal LiteralBase::Transpose(absl::Span<const int64_t> permutation) const {
  CHECK(shape().IsArray()) << "Tuple is not supported for transpose";
  CHECK(shape().rank() == permutation.size() && IsPermutation(permutation))
      << "Given permutation is not a permutation of dimension numbers";
  // To transpose the array, we just permute the dimensions and layout, and
  // do a straight memory copy of the raw data set.
  // This is considerably faster than iterating over every array element using
  // the EachCell<>() and Set<>() APIs.
  Shape permuted_shape = ShapeUtil::PermuteDimensions(permutation, shape());
  // Replace the layout with one affine to this shape, such that a
  // transpose operation can be performed by leaving the flat values
  // representation intact.
  // For example, consider the shape F32[11,8]{1,0} under a {1,0} permutation.
  // The shape with affine layout resulting from that operation will be
  // F32[8,11]{0,1}, since it leaves the original most minor (the 8 sized), the
  // most minor.
  //
  // Essentially, given MinMaj(Di) the position of the Di dimension within the
  // minor to major vector, and given T(Di) the index that the original Di
  // dimension has within the transposed array, a layout is affine if
  // MinMaj(Di) == TMinMaj(T(Di)), with TMinMaj() being the minor to major
  // vector of the affine layout.
  std::vector<int64_t> inverse_permutation = InversePermutation(permutation);
  CHECK(LayoutUtil::IsDenseArray(permuted_shape));
  Layout* layout = permuted_shape.mutable_layout();
  layout->clear_minor_to_major();
  for (auto index : LayoutUtil::MinorToMajor(shape())) {
    layout->add_minor_to_major(inverse_permutation[index]);
  }
  Literal new_literal(permuted_shape);
  for (int64_t i = 0; i < shape().rank(); i++) {
    new_literal.SetDynamicSize(inverse_permutation[i], GetDynamicSize(i));
  }
  DCHECK_EQ(ShapeUtil::ByteSizeOf(new_literal.shape()),
            ShapeUtil::ByteSizeOf(shape()));
  std::memcpy(new_literal.untyped_data(), untyped_data(), size_bytes());
  return new_literal;
}

template <typename NativeT>
Literal LiteralBase::SliceInternal(
    const Shape& result_shape, absl::Span<const int64_t> start_indices) const {
  Literal result_literal(result_shape);
  DimensionVector new_indices(result_shape.rank());
  CHECK(result_literal
            .Populate<NativeT>([&](absl::Span<const int64_t> indices) {
              for (int64_t i = 0; i < result_shape.rank(); ++i) {
                new_indices[i] = indices[i] + start_indices[i];
              }
              return Get<NativeT>(new_indices);
            })
            .ok());
  for (int64_t dnum = 0; dnum < shape().rank(); ++dnum) {
    if (shape().is_dynamic_dimension(dnum)) {
      int64_t dynamic_size = GetDynamicSize(dnum) - start_indices[dnum];
      CHECK_GE(dynamic_size, 0) << GetDynamicSize(dnum);
      dynamic_size = std::min(dynamic_size, result_shape.dimensions(dnum));
      result_literal.SetDynamicSize(dnum, dynamic_size);
    }
  }
  return result_literal;
}

Literal LiteralBase::Slice(absl::Span<const int64_t> start_indices,
                           absl::Span<const int64_t> limit_indices) const {
  CHECK(shape().IsArray()) << "tuple is not supported for slice";

  DimensionVector result_dimensions;
  for (int64_t dnum = 0; dnum < shape().rank(); ++dnum) {
    CHECK_GE(start_indices[dnum], 0);
    CHECK_LE(limit_indices[dnum], shape().dimensions(dnum))
        << "dnum = " << dnum;
    int64_t dimension = limit_indices[dnum] - start_indices[dnum];
    CHECK_GE(dimension, 0) << "dnum = " << dnum;
    result_dimensions.push_back(dimension);
  }
  auto result_shape =
      ShapeUtil::MakeShapeWithLayout(shape().element_type(), result_dimensions,
                                     LayoutUtil::MinorToMajor(shape()));
  ShapeUtil::CopyDynamicDimensions(&result_shape, shape());
  switch (result_shape.element_type()) {
    case PRED:
      return SliceInternal<bool>(result_shape, start_indices);
    case U8:
      return SliceInternal<uint8>(result_shape, start_indices);
    case U16:
      return SliceInternal<uint16>(result_shape, start_indices);
    case U32:
      return SliceInternal<uint32>(result_shape, start_indices);
    case U64:
      return SliceInternal<uint64_t>(result_shape, start_indices);
    case S8:
      return SliceInternal<int8>(result_shape, start_indices);
    case S16:
      return SliceInternal<int16>(result_shape, start_indices);
    case S32:
      return SliceInternal<int32>(result_shape, start_indices);
    case S64:
      return SliceInternal<int64_t>(result_shape, start_indices);
    case F16:
      return SliceInternal<half>(result_shape, start_indices);
    case BF16:
      return SliceInternal<bfloat16>(result_shape, start_indices);
    case F32:
      return SliceInternal<float>(result_shape, start_indices);
    case F64:
      return SliceInternal<double>(result_shape, start_indices);
    case C64:
      return SliceInternal<complex64>(result_shape, start_indices);
    case C128:
      return SliceInternal<complex128>(result_shape, start_indices);
    default:
      LOG(FATAL) << "not yet implemented: "
                 << PrimitiveType_Name(result_shape.element_type());
  }
}

Literal LiteralBase::Clone() const {
  Literal result(shape());
  TF_CHECK_OK(result.CopyFrom(*this));
  return result;
}

string LiteralBase::GetAsString(absl::Span<const int64_t> multi_index,
                                const ShapeIndex& shape_index) const {
  const Shape& subshape = ShapeUtil::GetSubshape(shape(), shape_index);
  CHECK(LayoutUtil::IsDenseArray(subshape));
  switch (subshape.element_type()) {
    case PRED:
      return Get<bool>(multi_index, shape_index) ? "true" : "false";
    case S8:
      return StrCat(Get<int8>(multi_index, shape_index));
    case S16:
      return StrCat(Get<int16>(multi_index, shape_index));
    case S32:
      return StrCat(Get<int32>(multi_index, shape_index));
    case S64:
      return StrCat(Get<int64_t>(multi_index, shape_index));
    case U8:
      return StrCat(Get<uint8>(multi_index, shape_index));
    case U16:
      return StrCat(Get<uint16>(multi_index, shape_index));
    case U32:
      return StrCat(Get<uint32>(multi_index, shape_index));
    case U64:
      return StrCat(Get<uint64_t>(multi_index, shape_index));
    case F16:
      return RoundTripFpToString(Get<half>(multi_index, shape_index));
    case F32:
      return RoundTripFpToString(Get<float>(multi_index, shape_index));
    case BF16:
      return RoundTripFpToString(Get<bfloat16>(multi_index, shape_index));
    case F64:
      return RoundTripFpToString(Get<double>(multi_index, shape_index));
    case C64: {
      complex64 c = Get<complex64>(multi_index, shape_index);
      return StrCat("(", RoundTripFpToString(c.real()), ", ",
                    RoundTripFpToString(c.imag()), ")");
    }
    case C128: {
      complex128 c = Get<complex128>(multi_index, shape_index);
      return StrCat("(", RoundTripFpToString(c.real()), ", ",
                    RoundTripFpToString(c.imag()), ")");
    }
    default:
      LOG(FATAL) << PrimitiveType_Name(subshape.element_type());
  }
}

absl::optional<int64_t> LiteralBase::GetIntegralAsS64(
    absl::Span<const int64_t> multi_index) const {
  CHECK(LayoutUtil::IsDenseArray(shape()));
  switch (shape().element_type()) {
    case PRED:
      return Get<bool>(multi_index);
    case S8:
      return Get<int8>(multi_index);
    case U8:
      return Get<uint8>(multi_index);
    case S16:
      return Get<int16>(multi_index);
    case U16:
      return Get<uint16>(multi_index);
    case S32:
      return Get<int32>(multi_index);
    case U32:
      return Get<uint32>(multi_index);
    case S64:
      return Get<int64_t>(multi_index);
    case U64:
      return Get<uint64_t>(multi_index);
    default:
      return absl::nullopt;
  }
}

absl::optional<double> LiteralBase::GetAsDouble(
    absl::Span<const int64_t> multi_index) const {
  CHECK(LayoutUtil::IsDenseArray(shape()));
  switch (shape().element_type()) {
    case F16:
      return static_cast<double>(Get<half>(multi_index));
    case F32:
      return static_cast<double>(Get<float>(multi_index));
    case F64:
      return Get<double>(multi_index);
    case BF16:
      return static_cast<double>(Get<bfloat16>(multi_index));
    default:
      return absl::nullopt;
  }
}

absl::optional<complex128> LiteralBase::GetAsComplex128(
    absl::Span<const int64_t> multi_index) const {
  switch (shape().element_type()) {
    case BF16:
      return {{static_cast<double>(Get<bfloat16>(multi_index)), 0}};
    case F16:
      return {{static_cast<double>(Get<Eigen::half>(multi_index)), 0}};
    case F32:
      return {{Get<float>(multi_index), 0}};
    case F64:
      return {{Get<double>(multi_index), 0}};
    case C64:
      return {Get<complex64>(multi_index)};
    case C128:
      return {Get<complex128>(multi_index)};
    case S8:
      return {Get<int8>(multi_index)};
    default:
      return absl::nullopt;
  }
}

size_t LiteralBase::Hash() const {
  using tensorflow::Hash64;
  using tensorflow::Hash64Combine;

  size_t hash_value = ShapeUtil::Hash(shape());

  ShapeUtil::ForEachSubshape(
      shape(), [&](const Shape& subshape, const ShapeIndex& index) {
        if (!subshape.IsArray()) {
          return;
        }

        CHECK(LayoutUtil::IsDense(subshape.layout()));
        hash_value = Hash64Combine(
            hash_value, Hash64(static_cast<const char*>(untyped_data(index)),
                               size_bytes(index)));
      });

  return hash_value;
}

Status MutableLiteralBase::SetIntegralAsS64(
    absl::Span<const int64_t> multi_index, int64_t value) {
  CHECK(LayoutUtil::IsDenseArray(shape()));
  switch (shape().element_type()) {
    case PRED:
      Set<bool>(multi_index, value);
      break;
    case U8:
      Set<uint8>(multi_index, value);
      break;
    case S32:
      Set<int32>(multi_index, value);
      break;
    case S64:
      Set<int64_t>(multi_index, value);
      break;
    case U32:
      Set<uint32>(multi_index, value);
      break;
    case U64:
      Set<uint64_t>(multi_index, value);
      break;
    default:
      return FailedPrecondition("Array element type is not integral: %s",
                                PrimitiveType_Name(shape().element_type()));
  }
  return Status::OK();
}

Status MutableLiteralBase::SetFromDouble(absl::Span<const int64_t> multi_index,
                                         double value) {
  CHECK(LayoutUtil::IsDenseArray(shape()));
  switch (shape().element_type()) {
    case F16:
      Set<half>(multi_index, Eigen::half(value));
      break;
    case F32:
      Set<float>(multi_index, value);
      break;
    case F64:
      Set<double>(multi_index, value);
      break;
    case BF16:
      Set<bfloat16>(multi_index, static_cast<bfloat16>(value));
      break;
    default:
      return FailedPrecondition("Array element type is not floating: %s",
                                PrimitiveType_Name(shape().element_type()));
  }
  return Status::OK();
}

namespace {

string ShapeToString(bool print_layout, const Shape& shape) {
  return print_layout ? ShapeUtil::HumanStringWithLayout(shape)
                      : ShapeUtil::HumanString(shape);
}

void ToStringHelper(const LiteralBase& literal, const ShapeIndex& shape_index,
                    bool print_shape, bool print_layout,
                    std::vector<string>* pieces);

void TupleToStringHelper(const LiteralBase& literal,
                         const ShapeIndex& shape_index, bool print_shape,
                         bool print_layout, std::vector<string>* pieces) {
  const Shape& subshape = ShapeUtil::GetSubshape(literal.shape(), shape_index);
  pieces->push_back("(\n");
  std::vector<string> tuple_pieces;
  for (int i = 0; i < ShapeUtil::TupleElementCount(subshape); ++i) {
    ShapeIndex element_index = shape_index;
    element_index.push_back(i);
    std::vector<string> element_pieces;
    ToStringHelper(literal, element_index, print_shape, print_layout,
                   &element_pieces);
    tuple_pieces.push_back(absl::StrJoin(element_pieces, ""));
  }
  pieces->push_back(absl::StrJoin(tuple_pieces, ",\n"));
  pieces->push_back("\n)");
}

void DenseArrayToStringHelper(const LiteralBase& literal,
                              const ShapeIndex& shape_index, bool print_shape,
                              bool print_layout, std::vector<string>* pieces) {
  const Shape& subshape = ShapeUtil::GetSubshape(literal.shape(), shape_index);
  int64_t rank = subshape.rank();

  std::function<void(absl::Span<const int64_t> dimensions,
                     std::vector<int64_t>*)>
      to_string_recursive = [&](absl::Span<const int64_t> dimensions,
                                std::vector<int64_t>* accum_indices) {
        // dimensions.size() decreases by 1 at each recursive call,
        // and accum_indices->size() increases by 1.
        // Their sum is equal to the rank of the tensor.
        CHECK_EQ(rank, dimensions.size() + accum_indices->size());

        auto brace_to_string = [&](string brace) -> string {
          // Handle 1D tensor
          if (rank == 1) {
            return brace;
          }
          // Handle the innermost tensor of a 2D+ tensor.
          if (dimensions.size() == 1 && brace == "{") {
            return StrCat("  ", brace, dimensions[0] <= 1 ? "" : " ");
          }
          if (dimensions.size() == 1 && brace == "}") {
            return StrCat(dimensions[0] <= 1 ? "" : " ", brace);
          }
          // Handle the non-innermost tensors of a 2D+ tensor.
          if (brace == "{") {
            const int64_t accum_indices_size = accum_indices->size();
            if (rank > 3 && !accum_indices->empty() &&
                accum_indices_size < rank) {
              int index = accum_indices->size() - 1;
              int value = accum_indices->back();
              return StrCat(brace, " /*i", index, "=", value, "*/\n");
            }
            return StrCat(brace, "\n");
          }
          return StrCat("\n", brace);
        };

        if (dimensions.empty()) {
          // Display predicates as 0s and 1s so that the string is more dense.
          string elem;
          if (subshape.element_type() == PRED && rank > 0) {
            elem = literal.Get<bool>(*accum_indices, shape_index) ? "1" : "0";
          } else {
            elem = literal.GetAsString(*accum_indices, shape_index);
          }
          pieces->push_back(elem);
        } else {
          pieces->push_back(brace_to_string("{"));
          for (int i = 0; i < dimensions[0]; ++i) {
            std::vector<int64_t> cloned_indices(*accum_indices);
            cloned_indices.push_back(i);
            to_string_recursive(dimensions.subspan(1), &cloned_indices);
            if (i < dimensions[0] - 1) {
              pieces->push_back(",");
              pieces->push_back(dimensions.size() > 1 ? "\n" : " ");
            }
          }
          pieces->push_back(brace_to_string("}"));
        }
      };

  if (print_shape) {
    pieces->push_back(ShapeToString(print_layout, subshape));
    if (subshape.is_dynamic()) {
      pieces->push_back("(");
      for (int64_t i = 0; i < subshape.dimensions_size(); ++i) {
        pieces->push_back(StrCat(literal.GetDynamicSize(i, shape_index)));
        if (i < subshape.dimensions_size() - 1) {
          pieces->push_back(",");
        }
      }
      pieces->push_back(")");
    }
    pieces->push_back(" ");
  }
  std::vector<int64_t> indices = {};
  std::vector<int64_t> dimensions;
  dimensions.reserve(subshape.rank());
  for (int64_t i = 0; i < subshape.rank(); ++i) {
    dimensions.push_back(literal.GetDynamicSize(i, shape_index));
  }
  to_string_recursive(dimensions, &indices);
}

void ToStringHelper(const LiteralBase& literal, const ShapeIndex& shape_index,
                    bool print_shape, bool print_layout,
                    std::vector<string>* pieces) {
  const Shape& subshape = ShapeUtil::GetSubshape(literal.shape(), shape_index);
  CHECK(LayoutUtil::HasLayout(literal.shape()));
  CHECK(LayoutUtil::HasLayout(subshape));
  if (subshape.IsTuple()) {
    TupleToStringHelper(literal, shape_index, print_shape, print_layout,
                        pieces);
  } else if (subshape.IsToken()) {
    pieces->push_back("token");
  } else {
    CHECK(LayoutUtil::IsDenseArray(subshape));
    DenseArrayToStringHelper(literal, shape_index, print_shape, print_layout,
                             pieces);
  }
}

}  // namespace

string LiteralBase::ToString() const {
  std::vector<string> pieces;
  CHECK(LayoutUtil::HasLayout(this->shape()));
  ToStringHelper(*this, {}, /*print_shape=*/true,
                 /*print_layout=*/false, &pieces);
  return absl::StrJoin(pieces, "");
}

string LiteralBase::ToStringOneline() const {
  return CompactOneline(ToString());
}

string LiteralBase::ToStringWithoutShape() const {
  std::vector<string> pieces;
  CHECK(LayoutUtil::HasLayout(this->shape()));
  ToStringHelper(*this, {}, /*print_shape=*/false,
                 /*print_layout=*/false, &pieces);
  return absl::StrJoin(pieces, "");
}

string LiteralBase::ToStringWithoutShapeOneline() const {
  return CompactOneline(ToStringWithoutShape());
}

string LiteralBase::ToStringWithLayout() const {
  std::vector<string> pieces;
  CHECK(LayoutUtil::HasLayout(this->shape()));
  ToStringHelper(*this, {}, /*print_shape=*/true,
                 /*print_layout=*/true, &pieces);
  return absl::StrJoin(pieces, "");
}

string LiteralBase::ToStringWithLayoutOneline() const {
  return CompactOneline(ToStringWithLayout());
}

void LiteralBase::EachCellAsString(
    const std::function<void(absl::Span<const int64_t> indices,
                             const string& value)>& per_cell) const {
  if (ShapeUtil::IsZeroElementArray(shape())) {
    return;
  }
  std::vector<int64_t> indices = IndexUtil::LinearIndexToMultidimensionalIndex(
      shape(), /*linear_index=*/0);
  do {
    per_cell(indices, GetAsString(indices));
  } while (IndexUtil::BumpIndices(shape(), absl::MakeSpan(indices)));
}

namespace {
template <typename NativeSrcT, typename NativeDestT, typename ConverterType>
Literal ConvertBetweenNativeTypesWithConverter(const LiteralBase& src_literal,
                                               const ConverterType& converter) {
  CHECK(src_literal.shape().IsArray());
  Literal result_literal(ShapeUtil::ChangeElementType(
      src_literal.shape(),
      primitive_util::NativeToPrimitiveType<NativeDestT>()));
  auto src_data = src_literal.data<NativeSrcT>();
  auto dest_data = result_literal.template data<NativeDestT>();
  int64_t num_elements = src_literal.element_count();

  for (int64_t i = 0; i < num_elements; ++i) {
    dest_data[i] = converter(src_data[i]);
  }
  return result_literal;
}

template <typename NativeSrcT, typename NativeDestT>
typename std::enable_if<std::is_same<NativeSrcT, Eigen::half>::value &&
                            (std::is_same<NativeDestT, complex64>::value ||
                             std::is_same<NativeDestT, complex128>::value),
                        Literal>::type
ConvertBetweenNativeTypes(const LiteralBase& src_literal) {
  auto converter = [](NativeSrcT src) {
    return NativeDestT(static_cast<typename NativeDestT::value_type>(src));
  };
  return ConvertBetweenNativeTypesWithConverter<NativeSrcT, NativeDestT>(
      src_literal, converter);
}

template <typename NativeSrcT, typename NativeDestT>
typename std::enable_if<std::is_floating_point<NativeSrcT>::value &&
                            std::is_integral<NativeDestT>::value,
                        Literal>::type
ConvertBetweenNativeTypes(const LiteralBase& src_literal) {
  auto converter = [](NativeSrcT src) {
    // C++ [conv.bool]p1:
    //   A prvalue of arithmetic [...] type can be converted to a prvalue of
    //   type bool. A zero value [...] is converted to false; any other value is
    //   converted to true.
    // C++ [conv.fpint]p1:
    //   [...] The behavior is undefined if the truncated value cannot be
    //   represented in the destination type.
    //
    // Using static_cast to convert a float to an integral type other than bool
    // may be undefined if the value's magnitude is too large or it is a NaN.
    // Let's choose saturating arithmetic as it captures the spirit of infinity
    // and arbitrarily map NaN to zero.
    if (!std::is_same<NativeDestT, bool>::value) {
      if (src != src) {
        return NativeDestT{0};
      }
      if (src >= std::numeric_limits<NativeDestT>::max()) {
        return std::numeric_limits<NativeDestT>::max();
      }
      if (src <= std::numeric_limits<NativeDestT>::lowest()) {
        return std::numeric_limits<NativeDestT>::lowest();
      }
    }
    return static_cast<NativeDestT>(src);
  };
  return ConvertBetweenNativeTypesWithConverter<NativeSrcT, NativeDestT>(
      src_literal, converter);
}

template <typename NativeSrcT, typename NativeDestT>
typename std::enable_if<!(std::is_floating_point<NativeSrcT>::value &&
                          std::is_integral<NativeDestT>::value) &&
                            !(std::is_same<NativeSrcT, Eigen::half>::value &&
                              (std::is_same<NativeDestT, complex64>::value ||
                               std::is_same<NativeDestT, complex128>::value)),
                        Literal>::type
ConvertBetweenNativeTypes(const LiteralBase& src_literal) {
  auto converter = [](NativeSrcT src) { return static_cast<NativeDestT>(src); };
  return ConvertBetweenNativeTypesWithConverter<NativeSrcT, NativeDestT>(
      src_literal, converter);
}

template <typename NativeSrcT, typename NativeDestT>
typename std::enable_if<(sizeof(NativeSrcT) == sizeof(NativeDestT) &&
                         !std::is_same<NativeDestT, Eigen::half>::value),
                        Literal>::type
BitcastBetweenNativeTypes(const LiteralBase& src_literal) {
  auto converter = [](NativeSrcT src) {
    return absl::bit_cast<NativeDestT>(GetRawValue(src));
  };
  return ConvertBetweenNativeTypesWithConverter<NativeSrcT, NativeDestT>(
      src_literal, converter);
}

template <typename NativeSrcT, typename NativeDestT>
typename std::enable_if<(sizeof(NativeSrcT) == sizeof(Eigen::half) &&
                         std::is_same<NativeDestT, Eigen::half>::value),
                        Literal>::type
BitcastBetweenNativeTypes(const LiteralBase& src_literal) {
  // Eigen::half doesn't satisfy the absl::bit_cast contract, so explicitly
  // cast to unsigned short first.
  auto converter = [](NativeSrcT src) {
    return Eigen::numext::bit_cast<Eigen::half>(
        absl::bit_cast<uint16>(GetRawValue(src)));
  };
  return ConvertBetweenNativeTypesWithConverter<NativeSrcT, Eigen::half>(
      src_literal, converter);
}

// This template specialization is here to make the compiler happy. bit_cast has
// a static check that the types are the same size. This specialization should
// never be used because the source and destination types are checked for
// identical sizes higher up.
template <typename NativeSrcT, typename NativeDestT>
typename std::enable_if<(sizeof(NativeSrcT) != sizeof(NativeDestT)),
                        Literal>::type
BitcastBetweenNativeTypes(const LiteralBase& src_literal) {
  LOG(FATAL) << "Invalid bitcast between types of different sizes.";
}

template <PrimitiveType primitive_src_type, PrimitiveType primitive_dest_type>
Literal ConvertIfTypesMatch(const LiteralBase& src_literal, bool bitcast) {
  CHECK_EQ(primitive_src_type, src_literal.shape().element_type());
  if (bitcast) {
    return BitcastBetweenNativeTypes<
        typename primitive_util::PrimitiveTypeToNative<
            primitive_src_type>::type,
        typename primitive_util::PrimitiveTypeToNative<
            primitive_dest_type>::type>(src_literal);
  } else {
    return ConvertBetweenNativeTypes<
        typename primitive_util::PrimitiveTypeToNative<
            primitive_src_type>::type,
        typename primitive_util::PrimitiveTypeToNative<
            primitive_dest_type>::type>(src_literal);
  }
}

template <PrimitiveType primitive_src_type>
StatusOr<Literal> ConvertIfDestTypeMatches(const LiteralBase& src_literal,
                                           PrimitiveType primitive_dest_type,
                                           bool bitcast) {
  switch (primitive_dest_type) {
#define CONVERT_IF_TYPES_MATCH(type)                                    \
  case (type):                                                          \
    return ConvertIfTypesMatch<primitive_src_type, (type)>(src_literal, \
                                                           bitcast);
    CONVERT_IF_TYPES_MATCH(PRED)
    CONVERT_IF_TYPES_MATCH(S8)
    CONVERT_IF_TYPES_MATCH(S16)
    CONVERT_IF_TYPES_MATCH(S32)
    CONVERT_IF_TYPES_MATCH(S64)
    CONVERT_IF_TYPES_MATCH(U8)
    CONVERT_IF_TYPES_MATCH(U16)
    CONVERT_IF_TYPES_MATCH(U32)
    CONVERT_IF_TYPES_MATCH(U64)
    CONVERT_IF_TYPES_MATCH(F16)
    CONVERT_IF_TYPES_MATCH(F32)
    CONVERT_IF_TYPES_MATCH(F64)
    CONVERT_IF_TYPES_MATCH(BF16)
#undef CONVERT_IF_TYPES_MATCH
    case C64:
      if (bitcast) {
        break;
      }
      return ConvertIfTypesMatch<primitive_src_type, C64>(src_literal, false);
    case C128:
      if (bitcast) {
        break;
      }
      return ConvertIfTypesMatch<primitive_src_type, C128>(src_literal, false);
    // Other types are not yet supported.
    default:
      break;
  }
  return Unimplemented("Converting from type %s to type %s is not implemented.",
                       PrimitiveType_Name(src_literal.shape().element_type()),
                       PrimitiveType_Name(primitive_dest_type));
}

StatusOr<Literal> ConvertSwitch(const LiteralBase& literal,
                                PrimitiveType primitive_dest_type,
                                bool bitcast) {
  TF_RET_CHECK(literal.shape().IsArray());
  if (literal.shape().element_type() == primitive_dest_type) {
    return literal.Clone();
  }
  switch (literal.shape().element_type()) {
#define CONVERT_IF_DEST_TYPE_MATCHES(type)                                \
  case (type):                                                            \
    return ConvertIfDestTypeMatches<(type)>(literal, primitive_dest_type, \
                                            bitcast);
    CONVERT_IF_DEST_TYPE_MATCHES(PRED)
    CONVERT_IF_DEST_TYPE_MATCHES(S8)
    CONVERT_IF_DEST_TYPE_MATCHES(S16)
    CONVERT_IF_DEST_TYPE_MATCHES(S32)
    CONVERT_IF_DEST_TYPE_MATCHES(S64)
    CONVERT_IF_DEST_TYPE_MATCHES(U8)
    CONVERT_IF_DEST_TYPE_MATCHES(U16)
    CONVERT_IF_DEST_TYPE_MATCHES(U32)
    CONVERT_IF_DEST_TYPE_MATCHES(U64)
    CONVERT_IF_DEST_TYPE_MATCHES(F16)
    CONVERT_IF_DEST_TYPE_MATCHES(F32)
    CONVERT_IF_DEST_TYPE_MATCHES(F64)
    CONVERT_IF_DEST_TYPE_MATCHES(BF16)
#undef CONVERT_IF_DEST_TYPE_MATCHES
      // Other types are not yet supported.
    default:
      return Unimplemented("%s from type %s to type %s is not implemented.",
                           (bitcast ? "Bitcast converting" : "Converting"),
                           PrimitiveType_Name(literal.shape().element_type()),
                           PrimitiveType_Name(primitive_dest_type));
  }
}

}  // namespace

StatusOr<Literal> LiteralBase::Convert(
    PrimitiveType primitive_dest_type) const {
  return ConvertSwitch(*this, primitive_dest_type, /*bitcast=*/false);
}

StatusOr<Literal> LiteralBase::BitcastConvert(const Shape& dest_shape) const {
  if (ShapeUtil::ByteSizeOf(dest_shape) != ShapeUtil::ByteSizeOf(shape())) {
    return InvalidArgument(
        "Can not bitcast-convert from shape %s to a shape of different size %s",
        shape().ToString(), dest_shape.ToString());
  }
  if (dest_shape.IsTuple() || shape().IsTuple()) {
    return InvalidArgument(
        "bitcast-convert is not valid for tuple shapes %s->%s",
        shape().ToString(), dest_shape.ToString());
  }
  if (shape().is_dynamic() || dest_shape.is_dynamic()) {
    return InvalidArgument(
        "bitcast-convert is not valid for dynamic shape %s->%s",
        shape().ToString(), dest_shape.ToString());
  }

  Literal out(dest_shape);
  std::memcpy(out.root_piece().buffer(), root_piece().buffer(),
              root_piece().size_bytes());
  return out;
}

StatusOr<Literal> LiteralBase::ConvertToShape(const Shape& dest_shape) const {
  if (!dest_shape.IsTuple()) {
    return Convert(dest_shape.element_type());
  }
  std::vector<Literal> elements;
  for (int i = 0; i < ShapeUtil::TupleElementCount(shape()); ++i) {
    auto element = LiteralSlice(*this, {i});
    TF_ASSIGN_OR_RETURN(
        auto new_element,
        element.ConvertToShape(ShapeUtil::GetSubshape(dest_shape, {i})));
    elements.push_back(std::move(new_element));
  }
  return MutableLiteralBase::MoveIntoTuple(absl::MakeSpan(elements));
}

/* static */ Literal MutableLiteralBase::MoveIntoTuple(
    absl::Span<Literal> elements) {
  std::vector<Shape> element_shapes;
  for (const Literal& element : elements) {
    element_shapes.push_back(element.shape());
  }
  Literal literal(ShapeUtil::MakeTupleShape(element_shapes),
                  /*allocate_arrays=*/false);
  for (int i = 0, end = elements.size(); i < end; ++i) {
    TF_CHECK_OK(
        literal.MoveFrom(std::move(elements[i]), /*dest_shape_index=*/{i}));
  }
  return literal;
}

template <typename NativeT>
void LiteralBase::Piece::CopyElementsWithDynamicBound(
    const LiteralBase::Piece& src) {
  auto dest_shape = subshape();
  auto src_shape = src.subshape();

  // At least one shape has to be static as bound.
  CHECK(dest_shape.is_static() || src_shape.is_static());
  auto bound_shape = dest_shape.is_static() ? src_shape : dest_shape;
  if (ShapeUtil::IsZeroElementArray(dest_shape)) {
    return;
  }
  std::vector<int64_t> index(dest_shape.rank());
  do {
    bool out_of_bound = false;
    for (int64_t i = 0; i < index.size(); ++i) {
      // Do not copy elements beyond dynamic bound.
      if (index[i] >= GetDynamicSize(i) || index[i] >= src.GetDynamicSize(i)) {
        out_of_bound = true;
      }
    }
    if (out_of_bound) {
      continue;
    }
    data<NativeT>()[IndexUtil::MultidimensionalIndexToLinearIndex(dest_shape,
                                                                  index)] =
        src.data<NativeT>()[IndexUtil::MultidimensionalIndexToLinearIndex(
            src_shape, index)];
  } while (IndexUtil::BumpIndices(bound_shape, absl::MakeSpan(index)));
}

template <typename NativeT>
bool LiteralBase::Piece::EqualElementsInternal(
    const LiteralBase::Piece& other, std::vector<int64_t>* multi_index) const {
  if (multi_index->size() == subshape().rank()) {
    return (Get<NativeT>(*multi_index) == other.Get<NativeT>(*multi_index));
  }
  for (int64_t i = 0; i < GetDynamicSize(multi_index->size()); ++i) {
    multi_index->push_back(i);
    if (!EqualElementsInternal<NativeT>(other, multi_index)) {
      return false;
    }
    multi_index->pop_back();
  }
  return true;
}

bool LiteralBase::Piece::EqualDynamicSize(
    const LiteralBase::Piece& other) const {
  DCHECK(ShapeUtil::Compatible(subshape(), other.subshape()));
  if (subshape().is_static()) {
    return true;
  }

  for (int64_t i = 0; i < subshape().rank(); ++i) {
    if (GetDynamicSize(i) != other.GetDynamicSize(i)) {
      return false;
    }
  }
  return true;
}

bool LiteralBase::Piece::EqualElements(const LiteralBase::Piece& other) const {
  if (subshape().is_static() &&
      ShapeUtil::Equal(subshape(), other.subshape()) &&
      LayoutUtil::IsDenseArray(subshape())) {
    CHECK_EQ(size_bytes(), other.size_bytes());
    return memcmp(buffer(), other.buffer(), size_bytes()) == 0;
  }

  std::vector<int64_t> multi_index;
  switch (subshape().element_type()) {
    case PRED:
      return EqualElementsInternal<bool>(other, &multi_index);
    case S8:
      return EqualElementsInternal<int8>(other, &multi_index);
    case S16:
      return EqualElementsInternal<int16>(other, &multi_index);
    case S32:
      return EqualElementsInternal<int32>(other, &multi_index);
    case S64:
      return EqualElementsInternal<int64_t>(other, &multi_index);
    case U8:
      return EqualElementsInternal<uint8>(other, &multi_index);
    case U16:
      return EqualElementsInternal<uint16>(other, &multi_index);
    case U32:
      return EqualElementsInternal<uint32>(other, &multi_index);
    case U64:
      return EqualElementsInternal<uint64_t>(other, &multi_index);
    case F32:
      return EqualElementsInternal<float>(other, &multi_index);
    case F64:
      return EqualElementsInternal<double>(other, &multi_index);
    case F16:
      return EqualElementsInternal<half>(other, &multi_index);
    case BF16:
      return EqualElementsInternal<bfloat16>(other, &multi_index);
    case C64:
      return EqualElementsInternal<complex64>(other, &multi_index);
    case C128:
      return EqualElementsInternal<complex128>(other, &multi_index);
    default:
      LOG(FATAL) << "Unimplemented: LiteralBase::Piece::EqualElements for type "
                 << PrimitiveType_Name(subshape().element_type());
  }
}

bool LiteralBase::operator==(const LiteralBase& other) const {
  // Checking the structure of tuple literals. Checks for dense arrays are
  // performed below.
  if (!ShapeUtil::EqualStructure(shape(), other.shape())) {
    return false;
  }

  return root_piece().ForEachSubpieceWithBool(
      [&](const ShapeIndex& index, const Piece& piece) {
        const Piece& other_piece = other.piece(index);
        const Shape& subshape = piece.subshape();
        const Shape& other_subshape = other_piece.subshape();
        if (subshape.element_type() != other_subshape.element_type()) {
          return false;
        }
        if (!piece.subshape().IsArray()) {
          return true;
        }
        if (subshape.rank() != other_subshape.rank()) {
          return false;
        }

        for (int64_t i = 0; i < subshape.rank(); ++i) {
          if (piece.GetDynamicSize(i) != other_piece.GetDynamicSize(i)) {
            return false;
          }
        }

        if (!piece.EqualElements(other_piece)) {
          return false;
        }
        return true;
      });
}

namespace {

template <typename NativeT>
static bool AllElementsEqualValue(absl::Span<const NativeT> data,
                                  NativeT value) {
  for (int64_t i = 0; i < data.size(); ++i) {
    if (data[i] != value) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool LiteralBase::IsAll(int8_t value) const {
  return root_piece().ForEachSubpieceWithBool([&](const ShapeIndex& index,
                                                  const Piece& piece) {
    if (!piece.subshape().IsArray()) {
      return true;
    }

    auto piece_is_all = [&]() {
      switch (shape().element_type()) {
        case U8:
          if (value >= 0) {
            return AllElementsEqualValue<uint8>(piece.data<uint8>(), value);
          }
          return false;
        case U16:
          if (value >= 0) {
            return AllElementsEqualValue<uint16>(piece.data<uint16>(), value);
          }
          return false;
        case U32:
          if (value >= 0) {
            return AllElementsEqualValue<uint32>(piece.data<uint32>(), value);
          }
          return false;
        case U64:
          if (value >= 0) {
            return AllElementsEqualValue<uint64_t>(piece.data<uint64_t>(),
                                                   value);
          }
          return false;
        case S8:
          return AllElementsEqualValue<int8>(piece.data<int8>(), value);
        case S16:
          return AllElementsEqualValue<int16>(piece.data<int16>(), value);
        case S32:
          return AllElementsEqualValue<int32>(piece.data<int32>(), value);
        case S64:
          return AllElementsEqualValue<int64_t>(piece.data<int64_t>(), value);
        case F32:
          return AllElementsEqualValue<float>(piece.data<float>(), value);
        case F64:
          return AllElementsEqualValue<double>(piece.data<double>(), value);
        case F16:
          return AllElementsEqualValue<half>(piece.data<half>(),
                                             static_cast<half>(value));
        case BF16:
          return AllElementsEqualValue<bfloat16>(piece.data<bfloat16>(),
                                                 static_cast<bfloat16>(value));
        case PRED:
          if (value == 0) {
            return AllElementsEqualValue<bool>(piece.data<bool>(), false);
          }
          if (value == 1) {
            return AllElementsEqualValue<bool>(piece.data<bool>(), true);
          }
          return false;
        default:
          return false;
      }
      return false;
    };

    if (!piece_is_all()) {
      return false;
    }
    return true;
  });
}

bool LiteralBase::IsAllFloat(float value) const {
  return root_piece().ForEachSubpieceWithBool(
      [&](const ShapeIndex& index, const Piece& piece) {
        if (!piece.subshape().IsArray()) {
          return true;
        }

        switch (shape().element_type()) {
          case F32:
            return AllElementsEqualValue<float>(piece.data<float>(), value);
          case F64:
            return AllElementsEqualValue<double>(piece.data<double>(), value);
          case F16:
            return AllElementsEqualValue<half>(piece.data<half>(),
                                               static_cast<half>(value));
          case BF16:
            return AllElementsEqualValue<bfloat16>(
                piece.data<bfloat16>(), static_cast<bfloat16>(value));
          default:
            return false;
        }
      });
}

bool LiteralBase::IsAllComplex(complex64 value) const {
  switch (shape().element_type()) {
    case C64:
      return AllElementsEqualValue<complex64>(root_piece().data<complex64>(),
                                              value);
    case C128:
      return AllElementsEqualValue<complex128>(root_piece().data<complex128>(),
                                               value);
    default:
      return false;
  }
}

bool LiteralBase::IsAllFirst() const {
  return root_piece().ForEachSubpieceWithBool(
      [&](const ShapeIndex& index, const Piece& piece) {
        if (!piece.subshape().IsArray()) {
          return true;
        }

        // Empty shapes are not all the first element since there is no first
        // element.
        if (ShapeUtil::IsZeroElementArray(piece.subshape())) {
          return false;
        }
        auto piece_is_all = [&]() {
          switch (piece.subshape().element_type()) {
            case PRED: {
              auto data = piece.data<bool>();
              return AllElementsEqualValue<bool>(data, data[0]);
            }
            // 8 bit types
            case S8: {
              auto data = piece.data<int8>();
              return AllElementsEqualValue<int8>(data, data[0]);
            }
            case U8: {
              auto data = piece.data<uint8>();
              return AllElementsEqualValue<uint8>(data, data[0]);
            }
            // 16 bit types
            case BF16: {
              auto data = piece.data<bfloat16>();
              return AllElementsEqualValue<bfloat16>(data, data[0]);
            }
            case F16: {
              auto data = piece.data<half>();
              return AllElementsEqualValue<half>(data, data[0]);
            }
            case S16: {
              auto data = piece.data<int16>();
              return AllElementsEqualValue<int16>(data, data[0]);
            }
            case U16: {
              auto data = piece.data<uint16>();
              return AllElementsEqualValue<uint16>(data, data[0]);
            }
            // 32 bit types
            case F32: {
              auto data = piece.data<float>();
              return AllElementsEqualValue<float>(data, data[0]);
            }
            case U32: {
              auto data = piece.data<uint32>();
              return AllElementsEqualValue<uint32>(data, data[0]);
            }
            case S32: {
              auto data = piece.data<int32>();
              return AllElementsEqualValue<int32>(data, data[0]);
            }
            // 64 bit types
            case C64: {
              auto data = piece.data<complex64>();
              return AllElementsEqualValue<complex64>(data, data[0]);
            }
            case F64: {
              auto data = piece.data<double>();
              return AllElementsEqualValue<double>(data, data[0]);
            }
            case S64: {
              auto data = piece.data<int64_t>();
              return AllElementsEqualValue<int64_t>(data, data[0]);
            }
            case U64: {
              auto data = piece.data<uint64_t>();
              return AllElementsEqualValue<uint64_t>(data, data[0]);
            }

            case C128: {
              auto data = piece.data<complex128>();
              return AllElementsEqualValue<complex128>(data, data[0]);
            }
            default:
              return false;
          }
        };

        if (!piece_is_all()) {
          return false;
        }
        return true;
      });
}

bool LiteralBase::IsR1Iota() const {
  if (!shape().IsArray()) {
    return false;
  }

  if (shape().rank() != 1) {
    return false;
  }

  auto is_iota_at_idx = [&](const int64_t idx) {
    switch (shape().element_type()) {
      case U8:
        return static_cast<int64_t>(Get<uint8>({idx})) == idx;
      case U16:
        return static_cast<int64_t>(Get<uint16>({idx})) == idx;
      case U32:
        return static_cast<int64_t>(Get<uint32>({idx})) == idx;
      case U64:
        return static_cast<int64_t>(Get<uint64_t>({idx})) == idx;
      case S8:
        return Get<int8>({idx}) == idx;
      case S16:
        return Get<int16>({idx}) == idx;
      case S32:
        return Get<int32>({idx}) == idx;
      case S64:
        return Get<int64_t>({idx}) == idx;
      case F32:
        return Get<float>({idx}) == idx;
      case F64:
        return Get<double>({idx}) == idx;
      case F16:
        return Get<half>({idx}) == static_cast<half>(idx);
      case BF16:
        return Get<bfloat16>({idx}) == static_cast<bfloat16>(idx);
      case C64:
        return Get<complex64>({idx}) == complex64(idx, 0.0f);
      case C128:
        return Get<complex128>({idx}) == complex128(idx, 0.0f);
      // pred, token, opaque, tuple, etc. are all not iota.
      default:
        return false;
    }
  };

  const int64_t elements = ShapeUtil::ElementsIn(shape());
  for (int64_t idx = 0; idx < elements; ++idx) {
    if (!is_iota_at_idx(idx)) {
      return false;
    }
  }

  return true;
}

// Returns a stride if the literal is a strided iota, i.e., iota multiplied by a
// stride. Only applicable for integer iotas. Returns absl::nullopt if the
// literal is not a strided iota.
absl::optional<int64_t> LiteralBase::IsR1StridedIota() const {
  if (!shape().IsArray() || shape().rank() != 1) {
    return absl::nullopt;
  }

  const int64_t elements = ShapeUtil::ElementsIn(shape());
  const PrimitiveType type = shape().element_type();
  if (elements <= 1 || !primitive_util::IsIntegralType(type)) {
    return absl::nullopt;
  }

  auto get_element_at = [&](const int64_t idx) -> int64_t {
    switch (type) {
      case U8:
        return static_cast<int64_t>(Get<uint8>({idx}));
      case U16:
        return static_cast<int64_t>(Get<uint16>({idx}));
      case U32:
        return static_cast<int64_t>(Get<uint32>({idx}));
      case U64:
        return static_cast<int64_t>(Get<uint64_t>({idx}));
      case S8:
        return Get<int8>({idx});
      case S16:
        return Get<int16>({idx});
      case S32:
        return Get<int32>({idx});
      case S64:
        return Get<int64_t>({idx});
      default:
        CHECK(0);
        return 0;
    }
  };

  // Infer the stride as the second element (since first element is supposed
  // to be zero).
  int64_t stride = get_element_at(1);
  if (stride == 0) {
    return absl::nullopt;
  }

  for (int64_t idx = 0; idx < elements; ++idx) {
    if (get_element_at(idx) != idx * stride) {
      return absl::nullopt;
    }
  }

  return stride;
}

bool LiteralBase::IsZero(absl::Span<const int64_t> indices) const {
  CHECK(shape().IsArray());
  switch (shape().element_type()) {
    case U8:
      return Get<uint8>(indices) == 0;
    case U16:
      return Get<uint16>(indices) == 0;
    case U32:
      return Get<uint32>(indices) == 0;
    case U64:
      return Get<uint64_t>(indices) == 0;
    case S8:
      return Get<int8>(indices) == 0;
    case S16:
      return Get<int16>(indices) == 0;
    case S32:
      return Get<int32>(indices) == 0;
    case S64:
      return Get<int64_t>(indices) == 0;
    case F32:
      return Get<float>(indices) == 0.0f;
    case F64:
      return Get<double>(indices) == 0.0;
    case C64:
      return Get<complex64>(indices) == complex64(0.0f, 0.0f);
    case C128:
      return Get<complex128>(indices) == complex128(0.0f, 0.0f);
    case F16:
      return Get<half>(indices) == static_cast<half>(0.0f);
    case BF16:
      return Get<bfloat16>(indices) == static_cast<bfloat16>(0.0f);
    case PRED:
      return Get<bool>(indices) == false;
    default:
      LOG(FATAL) << "Input literal must be an array.";
  }
}

namespace {

template <typename RepeatedFieldT, typename NativeT>
void CopyToRepeatedField(RepeatedFieldT* dest,
                         const absl::Span<const NativeT> src) {
  *dest = RepeatedFieldT(src.begin(), src.end());
}

}  // namespace

void LiteralBase::Piece::WriteToProto(LiteralProto* proto) const {
  *proto->mutable_shape() = subshape().ToProto();
  switch (subshape().element_type()) {
    case PRED:
      CopyToRepeatedField(proto->mutable_preds(), data<bool>());
      break;
    case S8:
      proto->set_s8s(static_cast<const signed char*>(data<int8>().data()),
                     element_count());
      break;
    case U8:
      proto->set_u8s(static_cast<const unsigned char*>(data<uint8>().data()),
                     element_count());
      break;
    case U32:
      CopyToRepeatedField(proto->mutable_u32s(), data<uint32>());
      break;
    case U64:
      CopyToRepeatedField(proto->mutable_u64s(), data<uint64_t>());
      break;
    case S32:
      CopyToRepeatedField(proto->mutable_s32s(), data<int32>());
      break;
    case S64:
      CopyToRepeatedField(proto->mutable_s64s(), data<int64_t>());
      break;
    case U16:
      *proto->mutable_u16s() = string(
          reinterpret_cast<const char*>(data<uint16_t>().data()), size_bytes());
      if (!kLittleEndian) {
        ConvertEndianShort(proto->mutable_u16s());
      }
      break;
    case S16:
      *proto->mutable_s16s() = string(
          reinterpret_cast<const char*>(data<int16_t>().data()), size_bytes());
      if (!kLittleEndian) {
        ConvertEndianShort(proto->mutable_s16s());
      }
      break;
    case F16:
      *proto->mutable_f16s() = string(
          reinterpret_cast<const char*>(data<half>().data()), size_bytes());
      if (!kLittleEndian) {
        ConvertEndianShort(proto->mutable_f16s());
      }
      break;
    case BF16:
      *proto->mutable_bf16s() = string(
          reinterpret_cast<const char*>(data<bfloat16>().data()), size_bytes());
      if (!kLittleEndian) {
        ConvertEndianShort(proto->mutable_bf16s());
      }
      break;
    case F32:
      CopyToRepeatedField(proto->mutable_f32s(), data<float>());
      break;
    case F64:
      CopyToRepeatedField(proto->mutable_f64s(), data<double>());
      break;
    case C64:
      for (complex64 value : data<complex64>()) {
        proto->add_c64s(value.real());
        proto->add_c64s(value.imag());
      }
      break;
    case C128:
      for (complex128 value : data<complex128>()) {
        proto->add_c128s(value.real());
        proto->add_c128s(value.imag());
      }
      break;
    case TUPLE:
    case TOKEN:
      // Nothing to do but assign the shape which is done above.
      return;
    default:
      // TODO(b/111551621): Support serializing more PrimitiveTypes.
      LOG(FATAL) << "Unhandled primitive type "
                 << PrimitiveType_Name(subshape().element_type());
  }
}

const void* LiteralBase::Piece::untyped_data() const {
  CHECK(subshape().IsArray()) << ShapeUtil::HumanString(subshape());
  return buffer();
}

void* LiteralBase::Piece::untyped_data() {
  CHECK(subshape().IsArray()) << ShapeUtil::HumanString(subshape());
  return buffer();
}

namespace {

template <typename RepeatedFieldT, typename NativeT>
Status CopyFromRepeatedField(absl::Span<NativeT> dest,
                             const RepeatedFieldT& src) {
  if (dest.size() != src.size()) {
    return InvalidArgument(
        "Expected %lu elements in LiteralProto repeated field, has %d",
        dest.size(), src.size());
  }
  std::copy(src.begin(), src.end(), dest.begin());
  return Status::OK();
}

}  // namespace

Status LiteralBase::Piece::CopyFromProto(const LiteralProto& proto) {
  // These conditions should have been checked in
  // MutableLiteralBase::CreateFromProto.
  TF_RET_CHECK(proto.has_shape());
  Shape shape(proto.shape());
  TF_RET_CHECK(LayoutUtil::HasLayout(shape));
  TF_RET_CHECK(ShapeUtil::Equal(shape, subshape()));

  switch (subshape().element_type()) {
    case PRED:
      TF_RETURN_IF_ERROR(CopyFromRepeatedField(data<bool>(), proto.preds()));
      break;
    case S8: {
      auto s8_data = data<int8>();
      TF_RET_CHECK(proto.s8s().size() == s8_data.size());
      std::copy(proto.s8s().begin(), proto.s8s().end(), s8_data.begin());
    } break;
    case U8: {
      auto u8_data = data<uint8>();
      TF_RET_CHECK(proto.u8s().size() == u8_data.size());
      std::copy(proto.u8s().begin(), proto.u8s().end(), u8_data.begin());
    } break;
    case S32:
      TF_RETURN_IF_ERROR(CopyFromRepeatedField(data<int32>(), proto.s32s()));
      break;
    case S64:
      TF_RETURN_IF_ERROR(CopyFromRepeatedField(data<int64_t>(), proto.s64s()));
      break;
    case U32:
      TF_RETURN_IF_ERROR(CopyFromRepeatedField(data<uint32>(), proto.u32s()));
      break;
    case U64:
      TF_RETURN_IF_ERROR(CopyFromRepeatedField(data<uint64_t>(), proto.u64s()));
      break;
    case S16: {
      const string& s(proto.s16s());
      TF_RET_CHECK(data<int16_t>().size() * sizeof(int16_t) == s.size());
      memcpy(untyped_data(), s.data(), s.size());
      if (!kLittleEndian) {
        ConvertEndianShort(reinterpret_cast<char*>(untyped_data()), s.size());
      }
    } break;
    case U16: {
      const string& s(proto.u16s());
      TF_RET_CHECK(data<uint16_t>().size() * sizeof(uint16_t) == s.size());
      memcpy(untyped_data(), s.data(), s.size());
      if (!kLittleEndian) {
        ConvertEndianShort(reinterpret_cast<char*>(untyped_data()), s.size());
      }
    } break;
    case F16: {
      const string& s(proto.f16s());
      TF_RET_CHECK(data<half>().size() * sizeof(half) == s.size());
      memcpy(untyped_data(), s.data(), s.size());
      if (!kLittleEndian) {
        ConvertEndianShort(reinterpret_cast<char*>(untyped_data()), s.size());
      }
    } break;

    case BF16: {
      const string& s(proto.bf16s());
      TF_RET_CHECK(data<bfloat16>().size() * sizeof(bfloat16) == s.size());
      memcpy(untyped_data(), s.data(), s.size());
      if (!kLittleEndian) {
        ConvertEndianShort(reinterpret_cast<char*>(untyped_data()), s.size());
      }
    } break;
    case F32:
      TF_RETURN_IF_ERROR(CopyFromRepeatedField(data<float>(), proto.f32s()));
      break;
    case F64:
      TF_RETURN_IF_ERROR(CopyFromRepeatedField(data<double>(), proto.f64s()));
      break;
    case C64: {
      auto complex_data = data<complex64>();
      TF_RET_CHECK(proto.c64s_size() == complex_data.size() * 2);
      for (int64_t i = 0; i < complex_data.size(); ++i) {
        complex_data[i] = complex64{proto.c64s(i * 2), proto.c64s(i * 2 + 1)};
      }
      break;
    }
    case C128: {
      auto complex_data = data<complex128>();
      const int64_t complex_data_size_doubled = complex_data.size() * 2;
      TF_RET_CHECK(proto.c128s_size() == complex_data_size_doubled);
      for (int64_t i = 0, end = complex_data.size(); i < end; ++i) {
        complex_data[i] =
            complex128{proto.c128s(i * 2), proto.c128s(i * 2 + 1)};
      }
      break;
    }
    case TUPLE:
      return InvalidArgument("Should not be called on tuple shapes: %s",
                             ShapeUtil::HumanString(subshape()));
    default:
      return InvalidArgument("Is called on unsupported shape: %s",
                             ShapeUtil::HumanString(subshape()));
  }
  return Status::OK();
}

LiteralProto LiteralBase::ToProto() const {
  LiteralProto proto;
  root_piece().ForEachSubpiece(
      [&](const ShapeIndex& index, const Piece& piece) {
        LiteralProto* proto_piece = &proto;
        for (int64_t i : index) {
          while (proto_piece->tuple_literals_size() <= i) {
            proto_piece->add_tuple_literals();
          }
          proto_piece = proto_piece->mutable_tuple_literals(i);
        }
        piece.WriteToProto(proto_piece);
      });

  return proto;
}

const void* LiteralBase::untyped_data(const ShapeIndex& shape_index) const {
  return piece(shape_index).untyped_data();
}

void* MutableLiteralBase::untyped_data(const ShapeIndex& shape_index) {
  return piece(shape_index).untyped_data();
}

int64_t LiteralBase::size_bytes(const ShapeIndex& shape_index) const {
  return piece(shape_index).size_bytes();
}

string LiteralBase::GetR1U8AsString() const {
  CHECK(shape().IsArray());
  CHECK_EQ(shape().rank(), 1);
  CHECK_EQ(shape().element_type(), U8);
  return string(absl::bit_cast<const char*>(data<uint8>().data()),
                ShapeUtil::ElementsIn(shape()));
}

void MutableBorrowingLiteral::CopyPieceSubtree(const Shape& shape,
                                               Piece* src_piece,
                                               Piece* dest_piece) {
  DCHECK(ShapeUtil::Equal(src_piece->subshape(), dest_piece->subshape()))
      << "src_piece has shape: "
      << ShapeUtil::HumanString(src_piece->subshape())
      << "dest_piece has shape: "
      << ShapeUtil::HumanString(dest_piece->subshape());
  if (shape.IsTuple()) {
    for (int i = 0; i < ShapeUtil::TupleElementCount(shape); ++i) {
      const Shape& subshape = shape.tuple_shapes(i);

      auto child_piece = Piece();
      child_piece.set_subshape(&subshape);

      CopyPieceSubtree(subshape, &src_piece->child(i), &child_piece);

      dest_piece->emplace_back(std::move(child_piece));
    }
  } else if (shape.IsArray()) {
    dest_piece->set_buffer(src_piece->buffer());
    dest_piece->set_dynamic_size_buffer(src_piece->dynamic_size_buffer());
  } else {
    // If the shape is neither an array nor tuple, then it must be
    // zero-sized. Otherwise, some memory needs to be allocated for it.
    CHECK_EQ(dest_piece->size_bytes(), 0);
  }
}

MutableLiteralBase::~MutableLiteralBase() {}

MutableBorrowingLiteral::MutableBorrowingLiteral(
    const MutableBorrowingLiteral& literal)
    : MutableLiteralBase() {
  shape_ = absl::make_unique<Shape>(literal.shape());
  CHECK(LayoutUtil::HasLayout(*shape_));

  root_piece_ = new Piece();
  root_piece_->set_subshape(shape_.get());

  CopyPieceSubtree(*shape_, &literal.root_piece(), root_piece_);
}

MutableBorrowingLiteral& MutableBorrowingLiteral::operator=(
    const MutableBorrowingLiteral& literal) {
  shape_ = absl::make_unique<Shape>(literal.shape());
  CHECK(LayoutUtil::HasLayout(*shape_));

  root_piece_ = new Piece();
  root_piece_->set_subshape(shape_.get());

  CopyPieceSubtree(*shape_, &literal.root_piece(), root_piece_);

  return *this;
}

MutableBorrowingLiteral::MutableBorrowingLiteral(MutableLiteralBase* literal)
    : MutableLiteralBase() {
  shape_ = absl::make_unique<Shape>(literal->shape());
  CHECK(LayoutUtil::HasLayout(*shape_));

  root_piece_ = new Piece();
  root_piece_->set_subshape(shape_.get());

  CopyPieceSubtree(*shape_, &literal->root_piece(), root_piece_);
}

MutableBorrowingLiteral::MutableBorrowingLiteral(
    MutableBorrowingLiteral literal, const ShapeIndex& view_root)
    : MutableLiteralBase() {
  shape_ = absl::make_unique<Shape>(literal.piece(view_root).subshape());
  CHECK(LayoutUtil::HasLayout(*shape_));

  root_piece_ = new Piece();
  root_piece_->set_subshape(shape_.get());

  CopyPieceSubtree(*shape_, &literal.piece(view_root), root_piece_);
}

MutableBorrowingLiteral::MutableBorrowingLiteral(const char* src_buf_ptr,
                                                 const Shape& shape)
    : MutableLiteralBase() {
  shape_ = absl::make_unique<Shape>(shape);
  CHECK(LayoutUtil::HasLayout(*shape_));
  CHECK(!shape_->IsTuple());

  root_piece_ = new Piece();
  root_piece_->set_buffer(const_cast<char*>(src_buf_ptr));
  root_piece_->set_subshape(shape_.get());
}

MutableBorrowingLiteral::MutableBorrowingLiteral(absl::Span<char*> src_buf_ptrs,
                                                 const Shape& shape)
    : MutableLiteralBase() {
  shape_ = absl::make_unique<Shape>(shape);
  if (!shape_->IsTuple()) {
    CHECK_EQ(src_buf_ptrs.size(), 1);
    root_piece_ = new Piece();
    root_piece_->set_buffer(const_cast<char*>(src_buf_ptrs[0]));
    root_piece_->set_subshape(shape_.get());
  } else {
    CHECK(!ShapeUtil::IsNestedTuple(*shape_));
    CHECK_EQ(src_buf_ptrs.size(), ShapeUtil::TupleElementCount(*shape_));
    root_piece_ = new Piece();
    root_piece_->set_subshape(shape_.get());

    for (int i = 0; i < src_buf_ptrs.size(); ++i) {
      Piece child_piece;
      const auto& src_shape = shape_->tuple_shapes(i);
      CHECK(src_shape.IsArray());
      child_piece.set_subshape(&src_shape);
      child_piece.set_buffer(src_buf_ptrs[i]);
      root_piece_->emplace_back(std::move(child_piece));
    }
  }
}

MutableBorrowingLiteral::~MutableBorrowingLiteral() {
  if (root_piece_ != nullptr) {
    delete root_piece_;
  }
}

LiteralSlice::LiteralSlice(const LiteralBase& literal)
    : LiteralBase(), root_piece_(&literal.root_piece()) {}

LiteralSlice::LiteralSlice(const LiteralBase& literal,
                           const ShapeIndex& view_root)
    : LiteralBase(), root_piece_(&literal.piece(view_root)) {}

void BorrowingLiteral::BuildPieceSubtree(const Shape& shape, Piece* piece) {
  CHECK(shape.IsTuple());
  for (int i = 0; i < ShapeUtil::TupleElementCount(shape); ++i) {
    const Shape& subshape = shape.tuple_shapes(i);

    auto child_piece = Piece();
    child_piece.set_subshape(&subshape);

    if (subshape.IsTuple()) {
      BuildPieceSubtree(subshape, &child_piece);
    }

    piece->emplace_back(std::move(child_piece));
  }
}

BorrowingLiteral::BorrowingLiteral(const char* src_buf_ptr, const Shape& shape)
    : LiteralBase(), shape_(absl::make_unique<Shape>(shape)) {
  CHECK(shape_->IsArray());
  CHECK(LayoutUtil::HasLayout(*shape_));

  root_piece_ = Piece();
  root_piece_.set_buffer(const_cast<char*>(src_buf_ptr));
  root_piece_.set_subshape(shape_.get());
}

BorrowingLiteral::BorrowingLiteral(absl::Span<const char* const> src_buf_ptrs,
                                   const Shape& shape)
    : LiteralBase(), shape_(absl::make_unique<Shape>(shape)) {
  CHECK(shape_->IsTuple());
  CHECK(!ShapeUtil::IsNestedTuple(*shape_));
  CHECK_EQ(src_buf_ptrs.size(), ShapeUtil::TupleElementCount(*shape_));
  root_piece_ = Piece();
  root_piece_.set_subshape(shape_.get());
  BuildPieceSubtree(*shape_, &root_piece_);

  for (int i = 0, end = src_buf_ptrs.size(); i < end; ++i) {
    const auto& src_shape = shape_->tuple_shapes(i);
    CHECK(src_shape.IsArray());
    root_piece_.child(i).set_buffer(const_cast<char*>(src_buf_ptrs[i]));
  }
}

}  // namespace xla
