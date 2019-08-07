// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_METADATA_MACROS_INTERNAL_H_
#define UI_VIEWS_METADATA_METADATA_MACROS_INTERNAL_H_

#include "base/compiler_specific.h"
#include "ui/views/metadata/metadata_cache.h"

// Internal Metadata Generation Helpers ---------------------------------------

#define METADATA_CLASS_NAME_INTERNAL(class_name) class_name##_MetaData
#define METADATA_FUNCTION_PREFIX_INTERNAL(class_name) \
  class_name::METADATA_CLASS_NAME_INTERNAL(class_name)

// Metadata Accessors ---------------------------------------------------------
#define METADATA_ACCESSORS_INTERNAL(class_name)      \
  static views::metadata::ClassMetaData* MetaData(); \
  views::metadata::ClassMetaData* GetClassMetaData() override;

// Metadata Class -------------------------------------------------------------
#define METADATA_CLASS_INTERNAL(class_name)                                  \
  class METADATA_CLASS_NAME_INTERNAL(class_name)                             \
      : public views::metadata::ClassMetaData {                              \
   public:                                                                   \
    explicit METADATA_CLASS_NAME_INTERNAL(class_name)() { BuildMetaData(); } \
                                                                             \
   private:                                                                  \
    friend class class_name;                                                 \
    virtual void BuildMetaData();                                            \
    static views::metadata::ClassMetaData* meta_data_ ALLOW_UNUSED_TYPE;     \
    DISALLOW_COPY_AND_ASSIGN(METADATA_CLASS_NAME_INTERNAL(class_name));      \
  }

#define METADATA_PROPERTY_TYPE_INTERNAL(class_name, property_type,        \
                                        property_name)                    \
  views::metadata::ClassPropertyMetaData<class_name, property_type,       \
                                         &class_name::Set##property_name, \
                                         &class_name::Get##property_name>

#endif  // UI_VIEWS_METADATA_METADATA_MACROS_INTERNAL_H_