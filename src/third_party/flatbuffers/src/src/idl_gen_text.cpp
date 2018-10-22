/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// independent from idl_parser, since this code is not needed for most clients

#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flexbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

namespace flatbuffers {

static bool GenStruct(const StructDef &struct_def, const Table *table,
                      int indent, const IDLOptions &opts, std::string *_text);

// If indentation is less than 0, that indicates we don't want any newlines
// either.
const char *NewLine(const IDLOptions &opts) {
  return opts.indent_step >= 0 ? "\n" : "";
}

int Indent(const IDLOptions &opts) { return std::max(opts.indent_step, 0); }

// Output an identifier with or without quotes depending on strictness.
void OutputIdentifier(const std::string &name, const IDLOptions &opts,
                      std::string *_text) {
  std::string &text = *_text;
  if (opts.strict_json) text += "\"";
  text += name;
  if (opts.strict_json) text += "\"";
}

// Print (and its template specialization below for pointers) generate text
// for a single FlatBuffer value into JSON format.
// The general case for scalars:
template<typename T>
bool Print(T val, Type type, int /*indent*/, Type * /*union_type*/,
           const IDLOptions &opts, std::string *_text) {
  std::string &text = *_text;
  if (type.enum_def && opts.output_enum_identifiers) {
    auto enum_val = type.enum_def->ReverseLookup(static_cast<int64_t>(val));
    if (enum_val) {
      text += "\"";
      text += enum_val->name;
      text += "\"";
      return true;
    }
  }

  if (type.base_type == BASE_TYPE_BOOL) {
    text += val != 0 ? "true" : "false";
  } else {
    text += NumToString(val);
  }

  return true;
}

// Print a vector a sequence of JSON values, comma separated, wrapped in "[]".
template<typename T>
bool PrintVector(const Vector<T> &v, Type type, int indent,
                 const IDLOptions &opts, std::string *_text) {
  std::string &text = *_text;
  text += "[";
  text += NewLine(opts);
  for (uoffset_t i = 0; i < v.size(); i++) {
    if (i) {
      if (!opts.protobuf_ascii_alike) text += ",";
      text += NewLine(opts);
    }
    text.append(indent + Indent(opts), ' ');
    if (IsStruct(type)) {
      if (!Print(v.GetStructFromOffset(i * type.struct_def->bytesize), type,
                 indent + Indent(opts), nullptr, opts, _text)) {
        return false;
      }
    } else {
      if (!Print(v[i], type, indent + Indent(opts), nullptr, opts, _text)) {
        return false;
      }
    }
  }
  text += NewLine(opts);
  text.append(indent, ' ');
  text += "]";
  return true;
}

// Specialization of Print above for pointer types.
template<>
bool Print<const void *>(const void *val, Type type, int indent,
                         Type *union_type, const IDLOptions &opts,
                         std::string *_text) {
  switch (type.base_type) {
    case BASE_TYPE_UNION:
      // If this assert hits, you have an corrupt buffer, a union type field
      // was not present or was out of range.
      FLATBUFFERS_ASSERT(union_type);
      return Print<const void *>(val, *union_type, indent, nullptr, opts,
                                 _text);
    case BASE_TYPE_STRUCT:
      if (!GenStruct(*type.struct_def, reinterpret_cast<const Table *>(val),
                     indent, opts, _text)) {
        return false;
      }
      break;
    case BASE_TYPE_STRING: {
      auto s = reinterpret_cast<const String *>(val);
      if (!EscapeString(s->c_str(), s->Length(), _text, opts.allow_non_utf8,
                        opts.natural_utf8)) {
        return false;
      }
      break;
    }
    case BASE_TYPE_VECTOR:
      type = type.VectorType();
      // Call PrintVector above specifically for each element type:
      switch (type.base_type) {
        // clang-format off
        #define FLATBUFFERS_TD(ENUM, IDLTYPE, \
          CTYPE, JTYPE, GTYPE, NTYPE, PTYPE) \
          case BASE_TYPE_ ## ENUM: \
            if (!PrintVector<CTYPE>( \
                  *reinterpret_cast<const Vector<CTYPE> *>(val), \
                  type, indent, opts, _text)) { \
              return false; \
            } \
            break;
          FLATBUFFERS_GEN_TYPES(FLATBUFFERS_TD)
        #undef FLATBUFFERS_TD
        // clang-format on
      }
      break;
    default: FLATBUFFERS_ASSERT(0);
  }
  return true;
}

// Generate text for a scalar field.
template<typename T> static bool GenField(const FieldDef &fd,
                                          const Table *table, bool fixed,
                                          const IDLOptions &opts,
                                          int indent,
                                          std::string *_text) {
  return Print(fixed ?
    reinterpret_cast<const Struct *>(table)->GetField<T>(fd.value.offset) :
    table->GetField<T>(fd.value.offset,
    IsFloat(fd.value.type.base_type) ?
    static_cast<T>(strtod(fd.value.constant.c_str(), nullptr)) :
    static_cast<T>(StringToInt(fd.value.constant.c_str()))),
    fd.value.type, indent, nullptr, opts, _text);
}

static bool GenStruct(const StructDef &struct_def, const Table *table,
                      int indent, const IDLOptions &opts, std::string *_text);

// Generate text for non-scalar field.
static bool GenFieldOffset(const FieldDef &fd, const Table *table, bool fixed,
                           int indent, Type *union_type, const IDLOptions &opts,
                           std::string *_text) {
  const void *val = nullptr;
  if (fixed) {
    // The only non-scalar fields in structs are structs.
    FLATBUFFERS_ASSERT(IsStruct(fd.value.type));
    val = reinterpret_cast<const Struct *>(table)->GetStruct<const void *>(
        fd.value.offset);
  } else if (fd.flexbuffer) {
    auto vec = table->GetPointer<const Vector<uint8_t> *>(fd.value.offset);
    auto root = flexbuffers::GetRoot(vec->data(), vec->size());
    root.ToString(true, opts.strict_json, *_text);
    return true;
  } else if (fd.nested_flatbuffer) {
    auto vec = table->GetPointer<const Vector<uint8_t> *>(fd.value.offset);
    auto root = GetRoot<Table>(vec->data());
    return GenStruct(*fd.nested_flatbuffer, root, indent, opts, _text);
  } else {
    val = IsStruct(fd.value.type)
              ? table->GetStruct<const void *>(fd.value.offset)
              : table->GetPointer<const void *>(fd.value.offset);
  }
  return Print(val, fd.value.type, indent, union_type, opts, _text);
}

// Generate text for a struct or table, values separated by commas, indented,
// and bracketed by "{}"
static bool GenStruct(const StructDef &struct_def, const Table *table,
                      int indent, const IDLOptions &opts, std::string *_text) {
  std::string &text = *_text;
  text += "{";
  int fieldout = 0;
  Type *union_type = nullptr;
  for (auto it = struct_def.fields.vec.begin();
       it != struct_def.fields.vec.end(); ++it) {
    FieldDef &fd = **it;
    auto is_present = struct_def.fixed || table->CheckField(fd.value.offset);
    auto output_anyway = opts.output_default_scalars_in_json &&
                         IsScalar(fd.value.type.base_type) && !fd.deprecated;
    if (is_present || output_anyway) {
      if (fieldout++) {
        if (!opts.protobuf_ascii_alike) text += ",";
      }
      text += NewLine(opts);
      text.append(indent + Indent(opts), ' ');
      OutputIdentifier(fd.name, opts, _text);
      if (!opts.protobuf_ascii_alike ||
          (fd.value.type.base_type != BASE_TYPE_STRUCT &&
           fd.value.type.base_type != BASE_TYPE_VECTOR))
        text += ":";
      text += " ";
      switch (fd.value.type.base_type) {
          // clang-format off
          #define FLATBUFFERS_TD(ENUM, IDLTYPE, \
            CTYPE, JTYPE, GTYPE, NTYPE, PTYPE) \
            case BASE_TYPE_ ## ENUM: \
              if (!GenField<CTYPE>(fd, table, struct_def.fixed, \
                                   opts, indent + Indent(opts), _text)) { \
                return false; \
              } \
              break;
          FLATBUFFERS_GEN_TYPES_SCALAR(FLATBUFFERS_TD)
        #undef FLATBUFFERS_TD
        // Generate drop-thru case statements for all pointer types:
        #define FLATBUFFERS_TD(ENUM, IDLTYPE, \
          CTYPE, JTYPE, GTYPE, NTYPE, PTYPE) \
          case BASE_TYPE_ ## ENUM:
          FLATBUFFERS_GEN_TYPES_POINTER(FLATBUFFERS_TD)
        #undef FLATBUFFERS_TD
            if (!GenFieldOffset(fd, table, struct_def.fixed, indent + Indent(opts),
                                union_type, opts, _text)) {
              return false;
            }
            break;
          // clang-format on
      }
      if (fd.value.type.base_type == BASE_TYPE_UTYPE) {
        auto enum_val = fd.value.type.enum_def->ReverseLookup(
            table->GetField<uint8_t>(fd.value.offset, 0));
        union_type = enum_val ? &enum_val->union_type : nullptr;
      }
    }
  }
  text += NewLine(opts);
  text.append(indent, ' ');
  text += "}";
  return true;
}

// Generate a text representation of a flatbuffer in JSON format.
bool GenerateText(const Parser &parser, const void *flatbuffer,
                  std::string *_text) {
  std::string &text = *_text;
  FLATBUFFERS_ASSERT(parser.root_struct_def_);  // call SetRootType()
  text.reserve(1024);               // Reduce amount of inevitable reallocs.
  auto root = parser.opts.size_prefixed ?
      GetSizePrefixedRoot<Table>(flatbuffer) : GetRoot<Table>(flatbuffer);
  if (!GenStruct(*parser.root_struct_def_, root, 0, parser.opts, _text)) {
    return false;
  }
  text += NewLine(parser.opts);
  return true;
}

std::string TextFileName(const std::string &path,
                         const std::string &file_name) {
  return path + file_name + ".json";
}

bool GenerateTextFile(const Parser &parser, const std::string &path,
                      const std::string &file_name) {
  if (!parser.builder_.GetSize() || !parser.root_struct_def_) return true;
  std::string text;
  if (!GenerateText(parser, parser.builder_.GetBufferPointer(), &text)) {
    return false;
  }
  return flatbuffers::SaveFile(TextFileName(path, file_name).c_str(), text,
                               false);
}

std::string TextMakeRule(const Parser &parser, const std::string &path,
                         const std::string &file_name) {
  if (!parser.builder_.GetSize() || !parser.root_struct_def_) return "";
  std::string filebase =
      flatbuffers::StripPath(flatbuffers::StripExtension(file_name));
  std::string make_rule = TextFileName(path, filebase) + ": " + file_name;
  auto included_files =
      parser.GetIncludedFilesRecursive(parser.root_struct_def_->file);
  for (auto it = included_files.begin(); it != included_files.end(); ++it) {
    make_rule += " " + *it;
  }
  return make_rule;
}

}  // namespace flatbuffers
