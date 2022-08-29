// Copyright 2020 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_signature.h"

#include <vector>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fxcrt/stl_util.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "third_party/base/numerics/safe_conversions.h"

namespace {

std::vector<CPDF_Dictionary*> CollectSignatures(CPDF_Document* doc) {
  std::vector<CPDF_Dictionary*> signatures;
  CPDF_Dictionary* root = doc->GetRoot();
  if (!root)
    return signatures;

  const CPDF_Dictionary* acro_form = root->GetDictFor("AcroForm");
  if (!acro_form)
    return signatures;

  const CPDF_Array* fields = acro_form->GetArrayFor("Fields");
  if (!fields)
    return signatures;

  CPDF_ArrayLocker locker(fields);
  for (auto& field : locker) {
    CPDF_Dictionary* field_dict = field->GetDict();
    if (field_dict && field_dict->GetNameFor("FT") == "Sig")
      signatures.push_back(field_dict);
  }
  return signatures;
}

}  // namespace

FPDF_EXPORT int FPDF_CALLCONV FPDF_GetSignatureCount(FPDF_DOCUMENT document) {
  auto* doc = CPDFDocumentFromFPDFDocument(document);
  if (!doc)
    return -1;

  return fxcrt::CollectionSize<int>(CollectSignatures(doc));
}

FPDF_EXPORT FPDF_SIGNATURE FPDF_CALLCONV
FPDF_GetSignatureObject(FPDF_DOCUMENT document, int index) {
  auto* doc = CPDFDocumentFromFPDFDocument(document);
  if (!doc)
    return nullptr;

  std::vector<CPDF_Dictionary*> signatures = CollectSignatures(doc);
  if (!fxcrt::IndexInBounds(signatures, index))
    return nullptr;

  return FPDFSignatureFromCPDFDictionary(signatures[index]);
}

FPDF_EXPORT unsigned long FPDF_CALLCONV
FPDFSignatureObj_GetContents(FPDF_SIGNATURE signature,
                             void* buffer,
                             unsigned long length) {
  CPDF_Dictionary* signature_dict = CPDFDictionaryFromFPDFSignature(signature);
  if (!signature_dict)
    return 0;

  CPDF_Dictionary* value_dict = signature_dict->GetDictFor("V");
  if (!value_dict)
    return 0;

  ByteString contents = value_dict->GetStringFor("Contents");
  const unsigned long contents_len =
      pdfium::base::checked_cast<unsigned long>(contents.GetLength());
  if (buffer && length >= contents_len)
    memcpy(buffer, contents.c_str(), contents_len);

  return contents_len;
}

FPDF_EXPORT unsigned long FPDF_CALLCONV
FPDFSignatureObj_GetByteRange(FPDF_SIGNATURE signature,
                              int* buffer,
                              unsigned long length) {
  CPDF_Dictionary* signature_dict = CPDFDictionaryFromFPDFSignature(signature);
  if (!signature_dict)
    return 0;

  CPDF_Dictionary* value_dict = signature_dict->GetDictFor("V");
  if (!value_dict)
    return 0;

  const CPDF_Array* byte_range = value_dict->GetArrayFor("ByteRange");
  if (!byte_range)
    return 0;

  const unsigned long byte_range_len =
      fxcrt::CollectionSize<unsigned long>(*byte_range);
  if (buffer && length >= byte_range_len) {
    for (size_t i = 0; i < byte_range_len; ++i)
      buffer[i] = byte_range->GetIntegerAt(i);
  }
  return byte_range_len;
}

FPDF_EXPORT unsigned long FPDF_CALLCONV
FPDFSignatureObj_GetSubFilter(FPDF_SIGNATURE signature,
                              char* buffer,
                              unsigned long length) {
  CPDF_Dictionary* signature_dict = CPDFDictionaryFromFPDFSignature(signature);
  if (!signature_dict)
    return 0;

  CPDF_Dictionary* value_dict = signature_dict->GetDictFor("V");
  if (!value_dict || !value_dict->KeyExist("SubFilter"))
    return 0;

  ByteString sub_filter = value_dict->GetNameFor("SubFilter");
  return NulTerminateMaybeCopyAndReturnLength(sub_filter, buffer, length);
}

FPDF_EXPORT unsigned long FPDF_CALLCONV
FPDFSignatureObj_GetReason(FPDF_SIGNATURE signature,
                           void* buffer,
                           unsigned long length) {
  CPDF_Dictionary* signature_dict = CPDFDictionaryFromFPDFSignature(signature);
  if (!signature_dict)
    return 0;

  CPDF_Dictionary* value_dict = signature_dict->GetDictFor("V");
  if (!value_dict)
    return 0;

  const CPDF_Object* obj = value_dict->GetObjectFor("Reason");
  if (!obj || !obj->IsString())
    return 0;

  return Utf16EncodeMaybeCopyAndReturnLength(obj->GetUnicodeText(), buffer,
                                             length);
}

FPDF_EXPORT unsigned long FPDF_CALLCONV
FPDFSignatureObj_GetTime(FPDF_SIGNATURE signature,
                         char* buffer,
                         unsigned long length) {
  CPDF_Dictionary* signature_dict = CPDFDictionaryFromFPDFSignature(signature);
  if (!signature_dict)
    return 0;

  CPDF_Dictionary* value_dict = signature_dict->GetDictFor("V");
  if (!value_dict)
    return 0;

  const CPDF_Object* obj = value_dict->GetObjectFor("M");
  if (!obj || !obj->IsString())
    return 0;

  return NulTerminateMaybeCopyAndReturnLength(obj->GetString(), buffer, length);
}

FPDF_EXPORT unsigned int FPDF_CALLCONV
FPDFSignatureObj_GetDocMDPPermission(FPDF_SIGNATURE signature) {
  int permission = 0;
  CPDF_Dictionary* signature_dict = CPDFDictionaryFromFPDFSignature(signature);
  if (!signature_dict)
    return permission;

  CPDF_Dictionary* value_dict = signature_dict->GetDictFor("V");
  if (!value_dict)
    return permission;

  CPDF_Array* references = value_dict->GetArrayFor("Reference");
  if (!references)
    return permission;

  CPDF_ArrayLocker locker(references);
  for (auto& reference : locker) {
    CPDF_Dictionary* reference_dict = reference->GetDict();
    if (!reference_dict)
      continue;

    ByteString transform_method = reference_dict->GetNameFor("TransformMethod");
    if (transform_method != "DocMDP")
      continue;

    CPDF_Dictionary* transform_params =
        reference_dict->GetDictFor("TransformParams");
    if (!transform_params)
      continue;

    // Valid values are 1, 2 and 3; 2 is the default.
    permission = transform_params->GetIntegerFor("P", 2);
    if (permission < 1 || permission > 3)
      permission = 0;

    return permission;
  }

  return permission;
}
