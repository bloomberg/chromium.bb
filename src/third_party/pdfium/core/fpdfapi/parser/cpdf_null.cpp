// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_null.h"

#include "core/fxcrt/fx_stream.h"
#include "third_party/base/ptr_util.h"

CPDF_Null::CPDF_Null() {}

CPDF_Object::Type CPDF_Null::GetType() const {
  return kNullobj;
}

RetainPtr<CPDF_Object> CPDF_Null::Clone() const {
  return pdfium::MakeRetain<CPDF_Null>();
}

bool CPDF_Null::WriteTo(IFX_ArchiveStream* archive,
                        const CPDF_Encryptor* encryptor) const {
  return archive->WriteString(" null");
}

bool CPDF_Null::IsNull() const {
  return true;
}
