// Copyright 2018 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/cpdf_object_stream.h"

#include <utility>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/cpdf_syntax_parser.h"
#include "core/fxcrt/cfx_readonlymemorystream.h"
#include "core/fxcrt/fx_safe_types.h"
#include "third_party/base/check.h"
#include "third_party/base/ptr_util.h"

namespace {

bool IsObjectStream(const CPDF_Object* object) {
  const CPDF_Stream* stream = ToStream(object);
  if (!stream)
    return false;

  const CPDF_Dictionary* stream_dict = stream->GetDict();
  if (!stream_dict)
    return false;

  if (stream_dict->GetNameFor("Type") != "ObjStm")
    return false;

  const CPDF_Number* number_of_objects =
      ToNumber(stream_dict->GetObjectFor("N"));
  if (!number_of_objects || !number_of_objects->IsInteger() ||
      number_of_objects->GetInteger() < 0 ||
      number_of_objects->GetInteger() >=
          static_cast<int>(CPDF_Parser::kMaxObjectNumber)) {
    return false;
  }

  const CPDF_Number* first_object_offset =
      ToNumber(stream_dict->GetObjectFor("First"));
  if (!first_object_offset || !first_object_offset->IsInteger() ||
      first_object_offset->GetInteger() < 0) {
    return false;
  }

  return true;
}

}  // namespace

//  static
std::unique_ptr<CPDF_ObjectStream> CPDF_ObjectStream::Create(
    const CPDF_Stream* stream) {
  if (!IsObjectStream(stream))
    return nullptr;

  // Protected constructor.
  return pdfium::WrapUnique(new CPDF_ObjectStream(stream));
}

CPDF_ObjectStream::CPDF_ObjectStream(const CPDF_Stream* obj_stream)
    : first_object_offset_(obj_stream->GetDict()->GetIntegerFor("First")) {
  DCHECK(IsObjectStream(obj_stream));
  Init(obj_stream);
}

CPDF_ObjectStream::~CPDF_ObjectStream() = default;

RetainPtr<CPDF_Object> CPDF_ObjectStream::ParseObject(
    CPDF_IndirectObjectHolder* pObjList,
    uint32_t obj_number,
    uint32_t archive_obj_index) const {
  if (archive_obj_index >= object_info_.size())
    return nullptr;

  const auto& info = object_info_[archive_obj_index];
  if (info.obj_num != obj_number)
    return nullptr;

  RetainPtr<CPDF_Object> result =
      ParseObjectAtOffset(pObjList, info.obj_offset);
  if (result)
    result->SetObjNum(obj_number);
  return result;
}

void CPDF_ObjectStream::Init(const CPDF_Stream* stream) {
  {
    auto stream_acc = pdfium::MakeRetain<CPDF_StreamAcc>(stream);
    stream_acc->LoadAllDataFiltered();
    const uint32_t data_size = stream_acc->GetSize();
    data_stream_ = pdfium::MakeRetain<CFX_ReadOnlyMemoryStream>(
        stream_acc->DetachData(), data_size);
  }

  CPDF_SyntaxParser syntax(data_stream_);
  const int object_count = stream->GetDict()->GetIntegerFor("N");
  for (int32_t i = object_count; i > 0; --i) {
    if (syntax.GetPos() >= data_stream_->GetSize())
      break;

    const uint32_t obj_num = syntax.GetDirectNum();
    const uint32_t obj_offset = syntax.GetDirectNum();
    if (!obj_num)
      continue;

    object_info_.push_back({obj_num, obj_offset});
  }
}

RetainPtr<CPDF_Object> CPDF_ObjectStream::ParseObjectAtOffset(
    CPDF_IndirectObjectHolder* pObjList,
    uint32_t object_offset) const {
  FX_SAFE_FILESIZE offset_in_stream = first_object_offset_;
  offset_in_stream += object_offset;

  if (!offset_in_stream.IsValid())
    return nullptr;

  if (offset_in_stream.ValueOrDie() >= data_stream_->GetSize())
    return nullptr;

  CPDF_SyntaxParser syntax(data_stream_);
  syntax.SetPos(offset_in_stream.ValueOrDie());
  return syntax.GetObjectBody(pObjList);
}
