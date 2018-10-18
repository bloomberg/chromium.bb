// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/codegen.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

// Convert '-' to '_' to use a CDDL identifier as a C identifier.
std::string ToUnderscoreId(const std::string& x) {
  std::string result(x);
  for (auto& c : result) {
    if (c == '-')
      c = '_';
  }
  return result;
}

// Convert a CDDL identifier to camel case for use as a C typename.  E.g.
// presentation-connection-message to PresentationConnectionMessage.
std::string ToCamelCase(const std::string& x) {
  std::string result(x);
  result[0] = toupper(result[0]);
  size_t new_size = 1;
  size_t result_size = result.size();
  for (size_t i = 1; i < result_size; ++i, ++new_size) {
    if (result[i] == '-') {
      ++i;
      if (i < result_size)
        result[new_size] = toupper(result[i]);
    } else {
      result[new_size] = result[i];
    }
  }
  result.resize(new_size);
  return result;
}

// Returns a string which represents the C++ type of |cpp_type|.  Returns an
// empty string if there is no valid representation for |cpp_type| (e.g. a
// vector with an invalid element type).
std::string CppTypeToString(const CppType& cpp_type) {
  switch (cpp_type.which) {
    case CppType::Which::kUint64:
      return "uint64_t";
    case CppType::Which::kString:
      return "std::string";
    case CppType::Which::kVector: {
      std::string element_string =
          CppTypeToString(*cpp_type.vector_type.element_type);
      if (element_string.empty())
        return std::string();
      return std::string("std::vector<") + element_string + std::string(">");
    } break;
    case CppType::Which::kEnum:
      return ToCamelCase(cpp_type.name);
    case CppType::Which::kStruct:
      return ToCamelCase(cpp_type.name);
    case CppType::Which::kTaggedType:
      return CppTypeToString(*cpp_type.tagged_type.real_type);
    default:
      return std::string();
  }
}

// Write the C++ struct member definitions of every type in |members| to the
// file descriptor |fd|.
bool WriteStructMembers(
    int fd,
    const std::vector<std::pair<std::string, CppType*>>& members) {
  for (const auto& x : members) {
    std::string type_string;
    switch (x.second->which) {
      case CppType::Which::kStruct: {
        if (x.second->struct_type.key_type ==
            CppType::Struct::KeyType::kPlainGroup) {
          if (!WriteStructMembers(fd, x.second->struct_type.members))
            return false;
          continue;
        } else {
          type_string = ToCamelCase(x.first);
        }
      } break;
      case CppType::Which::kOptional: {
        // TODO(btolsch): Make this optional<T> when one lands.
        dprintf(fd, "  bool has_%s;\n", ToUnderscoreId(x.first).c_str());
        type_string = CppTypeToString(*x.second->optional_type);
      } break;
      case CppType::Which::kDiscriminatedUnion: {
        // TODO(btolsch): Ctor/dtor when there's a d-union and
        // bool/uninitialized state when no uint present for this purpose.
        std::string cid = ToUnderscoreId(x.first);
        dprintf(fd, "  enum class Which%s {\n",
                ToCamelCase(x.second->name).c_str());
        for (auto* union_member : x.second->discriminated_union.members) {
          switch (union_member->which) {
            case CppType::Which::kUint64:
              dprintf(fd, "    kUint64,\n");
              break;
            case CppType::Which::kString:
              dprintf(fd, "    kString,\n");
              break;
            case CppType::Which::kBytes:
              dprintf(fd, "    kBytes,\n");
              break;
            default:
              return false;
          }
        }
        dprintf(fd, "  } which_%s;\n", cid.c_str());
        dprintf(fd, "  union {\n");
        for (auto* union_member : x.second->discriminated_union.members) {
          switch (union_member->which) {
            case CppType::Which::kUint64:
              dprintf(fd, "    uint64_t uint;\n");
              break;
            case CppType::Which::kString:
              dprintf(fd, "    std::string str;\n");
              break;
            case CppType::Which::kBytes:
              dprintf(fd, "    std::vector<uint8_t> bytes;\n");
              break;
            default:
              return false;
          }
        }
        dprintf(fd, "  } %s;\n", cid.c_str());
        return true;
      } break;
      default:
        type_string = CppTypeToString(*x.second);
        break;
    }
    if (type_string.empty())
      return false;
    dprintf(fd, "  %s %s;\n", type_string.c_str(),
            ToUnderscoreId(x.first).c_str());
  }
  return true;
}

// Writes a C++ type definition for |type| to the file descriptor |fd|.  This
// only generates a definition for enums and structs.
bool WriteTypeDefinition(int fd, const CppType& type) {
  switch (type.which) {
    case CppType::Which::kEnum: {
      dprintf(fd, "\nenum %s : uint64_t {\n", ToCamelCase(type.name).c_str());
      for (const auto& x : type.enum_type.members) {
        dprintf(fd, "  k%s = %luull,\n", ToCamelCase(x.first).c_str(),
                x.second);
      }
      dprintf(fd, "};\n");
    } break;
    case CppType::Which::kStruct: {
      dprintf(fd, "\nstruct %s {\n", ToCamelCase(type.name).c_str());
      if (!WriteStructMembers(fd, type.struct_type.members))
        return false;
      dprintf(fd, "};\n");
    } break;
    default:
      break;
  }
  return true;
}

// Ensures that any dependencies within |cpp_type| are written to the file
// descriptor |fd| before writing |cpp_type| to the file descriptor |fd|.  This
// is done by walking the tree of types defined by |cpp_type| (e.g. all the
// members for a struct).  |defs| contains the names of types that have already
// been written.  If a type hasn't been written and needs to be, its name will
// also be added to |defs|.
bool EnsureDependentTypeDefinitionsWritten(int fd,
                                           const CppType& cpp_type,
                                           std::set<std::string>* defs) {
  switch (cpp_type.which) {
    case CppType::Which::kVector: {
      return EnsureDependentTypeDefinitionsWritten(
          fd, *cpp_type.vector_type.element_type, defs);
    } break;
    case CppType::Which::kEnum: {
      if (defs->find(cpp_type.name) != defs->end())
        return true;
      for (const auto* x : cpp_type.enum_type.sub_members)
        if (!EnsureDependentTypeDefinitionsWritten(fd, *x, defs))
          return false;
      defs->emplace(cpp_type.name);
      WriteTypeDefinition(fd, cpp_type);
    } break;
    case CppType::Which::kStruct: {
      if (cpp_type.struct_type.key_type !=
          CppType::Struct::KeyType::kPlainGroup) {
        if (defs->find(cpp_type.name) != defs->end())
          return true;
        for (const auto& x : cpp_type.struct_type.members)
          if (!EnsureDependentTypeDefinitionsWritten(fd, *x.second, defs))
            return false;
        defs->emplace(cpp_type.name);
        WriteTypeDefinition(fd, cpp_type);
      }
    } break;
    case CppType::Which::kOptional: {
      return EnsureDependentTypeDefinitionsWritten(fd, *cpp_type.optional_type,
                                                   defs);
    } break;
    case CppType::Which::kDiscriminatedUnion: {
      for (const auto* x : cpp_type.discriminated_union.members)
        if (!EnsureDependentTypeDefinitionsWritten(fd, *x, defs))
          return false;
    } break;
    case CppType::Which::kTaggedType: {
      if (!EnsureDependentTypeDefinitionsWritten(
              fd, *cpp_type.tagged_type.real_type, defs)) {
        return false;
      }
    } break;
    default:
      break;
  }
  return true;
}

// Writes the type definition for every C++ type in |table|.  This function
// makes sure to write them in such an order that all type dependencies are
// written before they are need so the resulting text in the file descriptor
// |fd| will compile without modification.  For example, the following would be
// bad output:
//
// struct Foo {
//   Bar bar;
//   int x;
// };
//
// struct Bar {
//   int alpha;
// };
//
// This function ensures that Bar would be written sometime before Foo.
bool WriteTypeDefinitions(int fd, const CppSymbolTable& table) {
  std::set<std::string> defs;
  for (const auto& entry : table.cpp_type_map) {
    auto* type = entry.second;
    if (type->which != CppType::Which::kStruct ||
        type->struct_type.key_type == CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    if (!EnsureDependentTypeDefinitionsWritten(fd, *type, &defs))
      return false;
  }
  return true;
}

// Writes the function prototypes for the encode and decode functions for each
// type in |table| to the file descriptor |fd|.
bool WriteFunctionDeclarations(int fd, const CppSymbolTable& table) {
  for (const auto& entry : table.cpp_type_map) {
    const auto& name = entry.first;
    const auto* type = entry.second;
    if (type->which != CppType::Which::kStruct ||
        type->struct_type.key_type == CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    std::string cpp_name = ToCamelCase(name);
    dprintf(fd, "\nssize_t Encode%s(\n", cpp_name.c_str());
    dprintf(fd, "    const %s& data,\n", cpp_name.c_str());
    dprintf(fd, "    uint8_t* buffer,\n    size_t length);\n");
    dprintf(fd, "ssize_t Decode%s(\n", cpp_name.c_str());
    dprintf(fd, "    uint8_t* buffer,\n    size_t length,\n");
    dprintf(fd, "    %s* data);\n", cpp_name.c_str());
  }
  return true;
}

bool WriteMapEncoder(
    int fd,
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    const std::string& nested_type_scope,
    int encoder_depth = 1);
bool WriteArrayEncoder(
    int fd,
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    const std::string& nested_type_scope,
    int encoder_depth = 1);

// Writes the encoding function for the C++ type |cpp_type| to the file
// descriptor |fd|.  |name| is the C++ variable name that needs to be encoded.
// |nested_type_scope| is the closest C++ scope name (i.e. struct name), which
// may be used to access local enum constants.  |encoder_depth| is used to
// independently name independent cbor encoders that need to be created.
bool WriteEncoder(int fd,
                  const std::string& name,
                  const CppType& cpp_type,
                  const std::string& nested_type_scope,
                  int encoder_depth) {
  switch (cpp_type.which) {
    case CppType::Which::kStruct:
      if (cpp_type.struct_type.key_type == CppType::Struct::KeyType::kMap) {
        if (!WriteMapEncoder(fd, name, cpp_type.struct_type.members,
                             cpp_type.name, encoder_depth)) {
          return false;
        }
        return true;
      } else if (cpp_type.struct_type.key_type ==
                 CppType::Struct::KeyType::kArray) {
        if (!WriteArrayEncoder(fd, name, cpp_type.struct_type.members,
                               cpp_type.name, encoder_depth)) {
          return false;
        }
        return true;
      } else {
        for (const auto& x : cpp_type.struct_type.members) {
          dprintf(fd,
                  "  CBOR_RETURN_ON_ERROR(cbor_encode_text_string(&encoder%d, "
                  "\"%s\", sizeof(\"%s\") - 1));\n",
                  encoder_depth, x.first.c_str(), x.first.c_str());
          if (!WriteEncoder(fd,
                            name + std::string(".") + ToUnderscoreId(x.first),
                            *x.second, nested_type_scope, encoder_depth)) {
            return false;
          }
        }
        return true;
      }
      break;
    case CppType::Which::kUint64:
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder%d, %s));\n",
              encoder_depth, ToUnderscoreId(name).c_str());
      return true;
      break;
    case CppType::Which::kString: {
      std::string cid = ToUnderscoreId(name);
      dprintf(fd, "  if (!IsValidUtf8(%s)) {\n", cid.c_str());
      dprintf(fd, "    return -CborErrorInvalidUtf8TextString;\n");
      dprintf(fd, "  }\n");
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encode_text_string(&encoder%d, "
              "%s.c_str(), %s.size()));\n",
              encoder_depth, cid.c_str(), cid.c_str());
      return true;
    } break;
    case CppType::Which::kBytes: {
      std::string cid = ToUnderscoreId(name);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encode_byte_string(&encoder%d, "
              "%s.data(), "
              "%s.size()));\n",
              encoder_depth, cid.c_str(), cid.c_str());
      return true;
    } break;
    case CppType::Which::kVector: {
      std::string cid = ToUnderscoreId(name);
      dprintf(fd, "  CborEncoder encoder%d;\n", encoder_depth + 1);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_array(&encoder%d, "
              "&encoder%d, %s.size()));\n",
              encoder_depth, encoder_depth + 1, cid.c_str());
      dprintf(fd, "  for (const auto& x : %s) {\n", cid.c_str());
      if (!WriteEncoder(fd, "x", *cpp_type.vector_type.element_type,
                        nested_type_scope, encoder_depth + 1)) {
        return false;
      }
      dprintf(fd, "  }\n");
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder%d, "
              "&encoder%d));\n",
              encoder_depth, encoder_depth + 1);
      return true;
    } break;
    case CppType::Which::kEnum: {
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder%d, %s));\n",
              encoder_depth, ToUnderscoreId(name).c_str());
      return true;
    } break;
    case CppType::Which::kDiscriminatedUnion: {
      for (const auto* union_member : cpp_type.discriminated_union.members) {
        switch (union_member->which) {
          case CppType::Which::kUint64:
            dprintf(fd, "  case %s::Which%s::kUint64:\n",
                    ToCamelCase(nested_type_scope).c_str(),
                    ToCamelCase(union_member->name).c_str());
            if (!WriteEncoder(fd, ToUnderscoreId(name + std::string(".uint")),
                              *union_member, nested_type_scope,
                              encoder_depth)) {
              return false;
            }
            dprintf(fd, "    break;\n");
            break;
          case CppType::Which::kString:
            dprintf(fd, "  case %s::Which%s::kString:\n",
                    ToCamelCase(nested_type_scope).c_str(),
                    ToCamelCase(union_member->name).c_str());
            if (!WriteEncoder(fd, ToUnderscoreId(name + std::string(".str")),
                              *union_member, nested_type_scope,
                              encoder_depth)) {
              return false;
            }
            dprintf(fd, "    break;\n");
            break;
          case CppType::Which::kBytes:
            dprintf(fd, "  case %s::Which%s::kBytes:\n",
                    ToCamelCase(nested_type_scope).c_str(),
                    ToCamelCase(union_member->name).c_str());
            if (!WriteEncoder(fd, ToUnderscoreId(name + std::string(".bytes")),
                              *union_member, nested_type_scope,
                              encoder_depth)) {
              return false;
            }
            dprintf(fd, "    break;\n");
            break;
          default:
            return false;
        }
      }
      return true;
    } break;
    case CppType::Which::kTaggedType: {
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encode_tag(&encoder%d, %luull));\n",
              encoder_depth, cpp_type.tagged_type.tag);
      if (!WriteEncoder(fd, name, *cpp_type.tagged_type.real_type,
                        nested_type_scope, encoder_depth)) {
        return false;
      }
      return true;
    } break;
    default:
      break;
  }
  return false;
}

// Writes the encoding function for a CBOR map with the C++ type members in
// |members| to the file descriptor |fd|.  |name| is the C++ variable name that
// needs to be encoded.  |nested_type_scope| is the closest C++ scope name (i.e.
// struct name), which may be used to access local enum constants.
// |encoder_depth| is used to independently name independent cbor encoders that
// need to be created.
bool WriteMapEncoder(
    int fd,
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    const std::string& nested_type_scope,
    int encoder_depth) {
  dprintf(fd, "  CborEncoder encoder%d;\n", encoder_depth);
  dprintf(
      fd,
      "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_map(&encoder%d, &encoder%d, "
      "%d));\n",
      encoder_depth - 1, encoder_depth, static_cast<int>(members.size()));

  for (const auto& x : members) {
    std::string fullname = name;
    CppType* member_type = x.second;
    if (x.second->which != CppType::Which::kStruct ||
        x.second->struct_type.key_type !=
            CppType::Struct::KeyType::kPlainGroup) {
      if (x.second->which == CppType::Which::kOptional) {
        member_type = x.second->optional_type;
        dprintf(fd, "  if (%s.has_%s) {\n", ToUnderscoreId(name).c_str(),
                ToUnderscoreId(x.first).c_str());
      }
      dprintf(
          fd,
          "  CBOR_RETURN_ON_ERROR(cbor_encode_text_string(&encoder%d, \"%s\", "
          "sizeof(\"%s\") - 1));\n",
          encoder_depth, x.first.c_str(), x.first.c_str());
      if (x.second->which == CppType::Which::kDiscriminatedUnion) {
        dprintf(fd, "  switch (%s.which_%s) {\n", fullname.c_str(),
                x.first.c_str());
      }
      fullname = fullname + std::string(".") + x.first;
    }
    if (!WriteEncoder(fd, fullname, *member_type, nested_type_scope,
                      encoder_depth)) {
      return false;
    }
    if (x.second->which == CppType::Which::kOptional ||
        x.second->which == CppType::Which::kDiscriminatedUnion) {
      dprintf(fd, "  }\n");
    }
  }

  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder%d, "
          "&encoder%d));\n",
          encoder_depth - 1, encoder_depth);
  return true;
}

// Writes the encoding function for a CBOR array with the C++ type members in
// |members| to the file descriptor |fd|.  |name| is the C++ variable name that
// needs to be encoded.  |nested_type_scope| is the closest C++ scope name (i.e.
// struct name), which may be used to access local enum constants.
// |encoder_depth| is used to independently name independent cbor encoders that
// need to be created.
bool WriteArrayEncoder(
    int fd,
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    const std::string& nested_type_scope,
    int encoder_depth) {
  dprintf(fd, "  CborEncoder encoder%d;\n", encoder_depth);
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_array(&encoder%d, "
          "&encoder%d, %d));\n",
          encoder_depth - 1, encoder_depth, static_cast<int>(members.size()));

  for (const auto& x : members) {
    std::string fullname = name;
    CppType* member_type = x.second;
    if (x.second->which != CppType::Which::kStruct ||
        x.second->struct_type.key_type !=
            CppType::Struct::KeyType::kPlainGroup) {
      if (x.second->which == CppType::Which::kOptional) {
        member_type = x.second->optional_type;
        dprintf(fd, "  if (%s.has_%s) {\n", ToUnderscoreId(name).c_str(),
                ToUnderscoreId(x.first).c_str());
      }
      if (x.second->which == CppType::Which::kDiscriminatedUnion) {
        dprintf(fd, "  switch (%s.which_%s) {\n", fullname.c_str(),
                x.first.c_str());
      }
      fullname = fullname + std::string(".") + x.first;
    }
    if (!WriteEncoder(fd, fullname, *member_type, nested_type_scope,
                      encoder_depth)) {
      return false;
    }
    if (x.second->which == CppType::Which::kOptional ||
        x.second->which == CppType::Which::kDiscriminatedUnion) {
      dprintf(fd, "  }\n");
    }
  }

  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder%d, "
          "&encoder%d));\n",
          encoder_depth - 1, encoder_depth);
  return true;
}

// Writes encoding functions for each type in |table| to the file descriptor
// |fd|.
bool WriteEncoders(int fd, const CppSymbolTable& table) {
  for (const auto& entry : table.cpp_type_map) {
    const auto& name = entry.first;
    const auto* type = entry.second;
    if (type->which != CppType::Which::kStruct ||
        type->struct_type.key_type == CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    std::string cpp_name = ToCamelCase(name);
    dprintf(fd, "\nssize_t Encode%s(\n", cpp_name.c_str());
    dprintf(fd, "    const %s& data,\n", cpp_name.c_str());
    dprintf(fd, "    uint8_t* buffer,\n    size_t length) {\n");
    dprintf(fd, "  CborEncoder encoder0;\n");
    dprintf(fd, "  cbor_encoder_init(&encoder0, buffer, length, 0);\n");

    if (type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
      if (!WriteMapEncoder(fd, "data", type->struct_type.members, type->name))
        return false;
    } else {
      if (!WriteArrayEncoder(fd, "data", type->struct_type.members,
                             type->name)) {
        return false;
      }
    }

    dprintf(fd,
            "  size_t extra_bytes_needed = "
            "cbor_encoder_get_extra_bytes_needed(&encoder0);\n");
    dprintf(fd, "  if (extra_bytes_needed) {\n");
    dprintf(fd,
            "    return static_cast<ssize_t>(length + extra_bytes_needed);\n");
    dprintf(fd, "  } else {\n");
    dprintf(fd,
            "    return "
            "static_cast<ssize_t>(cbor_encoder_get_buffer_size(&encoder0, "
            "buffer));\n");
    dprintf(fd, "  }\n");
    dprintf(fd, "}\n");
  }
  return true;
}

bool WriteMapDecoder(
    int fd,
    const std::string& name,
    const std::string& member_accessor,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int decoder_depth,
    int* temporary_count);
bool WriteArrayDecoder(
    int fd,
    const std::string& name,
    const std::string& member_accessor,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int decoder_depth,
    int* temporary_count);

// Writes the decoding function for the C++ type |cpp_type| to the file
// descriptor |fd|.  |name| is the C++ variable name that needs to be encoded.
// |member_accessor| is either "." or "->" depending on whether |name| is a
// pointer type.  |decoder_depth| is used to independently name independent cbor
// decoders that need to be created.  |temporary_count| is used to ensure
// temporaries get unique names by appending an automatically incremented
// integer.
bool WriteDecoder(int fd,
                  const std::string& name,
                  const std::string& member_accessor,
                  const CppType& cpp_type,
                  int decoder_depth,
                  int* temporary_count) {
  switch (cpp_type.which) {
    case CppType::Which::kUint64: {
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_get_uint64(&it%d, &%s));\n",
              decoder_depth, name.c_str());
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it%d));\n",
              decoder_depth);
      return true;
    } break;
    case CppType::Which::kString: {
      int temp_length = (*temporary_count)++;
      dprintf(fd, "  size_t length%d = 0;", temp_length);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_validate(&it%d, "
              "CborValidateUtf8));\n",
              decoder_depth);
      dprintf(fd, "  if (cbor_value_is_length_known(&it%d)) {\n",
              decoder_depth);
      dprintf(fd,
              "    CBOR_RETURN_ON_ERROR(cbor_value_get_string_length(&it%d, "
              "&length%d));\n",
              decoder_depth, temp_length);
      dprintf(fd, "  } else {\n");
      dprintf(
          fd,
          "    CBOR_RETURN_ON_ERROR(cbor_value_calculate_string_length(&it%d, "
          "&length%d));\n",
          decoder_depth, temp_length);
      dprintf(fd, "  }\n");
      dprintf(fd, "  %s%sresize(length%d);\n", name.c_str(),
              member_accessor.c_str(), temp_length);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_copy_text_string(&it%d, "
              "const_cast<char*>(%s%sdata()), &length%d, nullptr));\n",
              decoder_depth, name.c_str(), member_accessor.c_str(),
              temp_length);
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance(&it%d));\n",
              decoder_depth);
      return true;
    } break;
    case CppType::Which::kBytes: {
      int temp_length = (*temporary_count)++;
      dprintf(fd, "  size_t length%d = 0;", temp_length);
      dprintf(fd, "  if (cbor_value_is_length_known(&it%d)) {\n",
              decoder_depth);
      dprintf(fd,
              "    CBOR_RETURN_ON_ERROR(cbor_value_get_string_length(&it%d, "
              "&length%d));\n",
              decoder_depth, temp_length);
      dprintf(fd, "  } else {\n");
      dprintf(
          fd,
          "    CBOR_RETURN_ON_ERROR(cbor_value_calculate_string_length(&it%d, "
          "&length%d));\n",
          decoder_depth, temp_length);
      dprintf(fd, "  }\n");
      dprintf(fd, "  %s%sresize(length%d);\n", name.c_str(),
              member_accessor.c_str(), temp_length);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_copy_byte_string(&it%d, "
              "const_cast<uint8_t*>(%s%sdata()), &length%d, nullptr));\n",
              decoder_depth, name.c_str(), member_accessor.c_str(),
              temp_length);
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance(&it%d));\n",
              decoder_depth);
      return true;
    } break;
    case CppType::Which::kVector: {
      dprintf(fd, "  if (cbor_value_get_type(&it%d) != CborArrayType) {\n",
              decoder_depth);
      dprintf(fd, "    return -1;\n");
      dprintf(fd, "  }\n");
      dprintf(fd, "  CborValue it%d;\n", decoder_depth + 1);
      dprintf(fd, "  size_t it%d_length = 0;\n", decoder_depth + 1);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_get_array_length(&it%d, "
              "&it%d_length));\n",
              decoder_depth, decoder_depth + 1);
      dprintf(fd, "  %s%sresize(it%d_length);\n", name.c_str(),
              member_accessor.c_str(), decoder_depth + 1);
      dprintf(
          fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it%d, &it%d));\n",
          decoder_depth, decoder_depth + 1);
      dprintf(fd, "  for (auto i = %s%sbegin(); i != %s%send(); ++i) {\n",
              name.c_str(), member_accessor.c_str(), name.c_str(),
              member_accessor.c_str());
      if (!WriteDecoder(fd, "(*i)", ".", *cpp_type.vector_type.element_type,
                        decoder_depth + 1, temporary_count)) {
        return false;
      }
      dprintf(fd, "  }\n");
      dprintf(
          fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it%d, &it%d));\n",
          decoder_depth, decoder_depth + 1);
      return true;
    } break;
    case CppType::Which::kEnum: {
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_get_uint64(&it%d, "
              "reinterpret_cast<uint64_t*>(&%s)));\n",
              decoder_depth, name.c_str());
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it%d));\n",
              decoder_depth);
      // TODO(btolsch): Validate against enum members.
      return true;
    } break;
    case CppType::Which::kStruct: {
      if (cpp_type.struct_type.key_type == CppType::Struct::KeyType::kMap) {
        return WriteMapDecoder(fd, name, member_accessor,
                               cpp_type.struct_type.members, decoder_depth + 1,
                               temporary_count);
      } else if (cpp_type.struct_type.key_type ==
                 CppType::Struct::KeyType::kArray) {
        return WriteArrayDecoder(fd, name, member_accessor,
                                 cpp_type.struct_type.members,
                                 decoder_depth + 1, temporary_count);
      }
    } break;
    case CppType::Which::kDiscriminatedUnion: {
      int temp_value_type = (*temporary_count)++;
      dprintf(fd, "  CborType type%d = cbor_value_get_type(&it%d);\n",
              temp_value_type, decoder_depth);
      bool first = true;
      for (const auto* x : cpp_type.discriminated_union.members) {
        if (first)
          first = false;
        else
          dprintf(fd, " else ");
        switch (x->which) {
          case CppType::Which::kUint64:
            dprintf(fd,
                    "  if (type%d == CborIntegerType && (it%d.flags & "
                    "CborIteratorFlag_NegativeInteger) == 0) {\n",
                    temp_value_type, decoder_depth);
            // TODO(btolsch): assign which_*.
            if (!WriteDecoder(fd, name + std::string(".uint"), ".", *x,
                              decoder_depth, temporary_count)) {
              return false;
            }
            break;
          case CppType::Which::kString:
            dprintf(fd, "  if (type%d == CborTextStringType) {\n",
                    temp_value_type);
            if (!WriteDecoder(fd, name + std::string(".str"), ".", *x,
                              decoder_depth, temporary_count)) {
              return false;
            }
            break;
          case CppType::Which::kBytes:
            dprintf(fd, "  if (type%d == CborByteStringType) {\n",
                    temp_value_type);
            if (!WriteDecoder(fd, name + std::string(".bytes"), ".", *x,
                              decoder_depth, temporary_count)) {
              return false;
            }
            break;
          default:
            return false;
        }
        dprintf(fd, "  }\n");
      }
      dprintf(fd, " else { return -1; }\n");
      return true;
    } break;
    case CppType::Which::kTaggedType: {
      int temp_tag = (*temporary_count)++;
      dprintf(fd, "  uint64_t tag%d = 0;\n", temp_tag);
      dprintf(fd, "  cbor_value_get_tag(&it%d, &tag%d);\n", decoder_depth,
              temp_tag);
      dprintf(fd, "  if (tag%d != %luull) {\n", temp_tag,
              cpp_type.tagged_type.tag);
      dprintf(fd, "    return -1;\n");
      dprintf(fd, "  }\n");
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it%d));\n",
              decoder_depth);
      if (!WriteDecoder(fd, name, member_accessor,
                        *cpp_type.tagged_type.real_type, decoder_depth,
                        temporary_count)) {
        return false;
      }
      return true;
    } break;
    default:
      break;
  }
  return false;
}

// Writes the decoding function for the CBOR map with members in |members| to
// the file descriptor |fd|.  |name| is the C++ variable name that needs to be
// encoded.  |member_accessor| is either "." or "->" depending on whether |name|
// is a pointer type.  |decoder_depth| is used to independently name independent
// cbor decoders that need to be created.  |temporary_count| is used to ensure
// temporaries get unique names by appending an automatically incremented
// integer.
bool WriteMapDecoder(
    int fd,
    const std::string& name,
    const std::string& member_accessor,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int decoder_depth,
    int* temporary_count) {
  dprintf(fd, "  if (cbor_value_get_type(&it%d) != CborMapType) {\n",
          decoder_depth - 1);
  dprintf(fd, "    return -1;\n");
  dprintf(fd, "  }\n");
  dprintf(fd, "  CborValue it%d;\n", decoder_depth);
  dprintf(fd, "  size_t it%d_length = 0;\n", decoder_depth);
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_get_map_length(&it%d, "
          "&it%d_length));\n",
          decoder_depth - 1, decoder_depth);
  // TODO(btolsch): Account for optional combinations.
  dprintf(fd, "  if (it%d_length != %d) {\n", decoder_depth,
          static_cast<int>(members.size()));
  dprintf(fd, "    return -1;\n");
  dprintf(fd, "  }\n");
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it%d, &it%d));\n",
          decoder_depth - 1, decoder_depth);
  int member_pos = 0;
  for (const auto& x : members) {
    std::string cid = ToUnderscoreId(x.first);
    std::string fullname = name + member_accessor + cid;
    dprintf(fd, "  CBOR_RETURN_ON_ERROR(EXPECT_KEY_CONSTANT(&it%d, \"%s\"));\n",
            decoder_depth, x.first.c_str());
    if (x.second->which == CppType::Which::kOptional) {
      dprintf(fd, "  if (it%d_length > %d) {\n", decoder_depth, member_pos);
      dprintf(fd, "    %s%shas_%s = true;\n", name.c_str(),
              member_accessor.c_str(), cid.c_str());
      if (!WriteDecoder(fd, fullname, ".", *x.second->optional_type,
                        decoder_depth, temporary_count)) {
        return false;
      }
      dprintf(fd, "  } else {\n");
      dprintf(fd, "    %s%shas_%s = false;\n", name.c_str(),
              member_accessor.c_str(), cid.c_str());
      dprintf(fd, "  }\n");
    } else {
      if (!WriteDecoder(fd, fullname, ".", *x.second, decoder_depth,
                        temporary_count)) {
        return false;
      }
    }
    ++member_pos;
  }
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it%d, &it%d));\n",
          decoder_depth - 1, decoder_depth);
  return true;
}

// Writes the decoding function for the CBOR array with members in |members| to
// the file descriptor |fd|.  |name| is the C++ variable name that needs to be
// encoded.  |member_accessor| is either "." or "->" depending on whether |name|
// is a pointer type.  |decoder_depth| is used to independently name independent
// cbor decoders that need to be created.  |temporary_count| is used to ensure
// temporaries get unique names by appending an automatically incremented
// integer.
bool WriteArrayDecoder(
    int fd,
    const std::string& name,
    const std::string& member_accessor,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int decoder_depth,
    int* temporary_count) {
  dprintf(fd, "  if (cbor_value_get_type(&it%d) != CborArrayType) {\n",
          decoder_depth - 1);
  dprintf(fd, "    return -1;\n");
  dprintf(fd, "  }\n");
  dprintf(fd, "  CborValue it%d;\n", decoder_depth);
  dprintf(fd, "  size_t it%d_length = 0;\n", decoder_depth);
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_get_array_length(&it%d, "
          "&it%d_length));\n",
          decoder_depth - 1, decoder_depth);
  // TODO(btolsch): Account for optional combinations.
  dprintf(fd, "  if (it%d_length != %d) {\n", decoder_depth,
          static_cast<int>(members.size()));
  dprintf(fd, "    return -1;\n");
  dprintf(fd, "  }\n");
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it%d, &it%d));\n",
          decoder_depth - 1, decoder_depth);
  int member_pos = 0;
  for (const auto& x : members) {
    std::string cid = ToUnderscoreId(x.first);
    std::string fullname = name + member_accessor + cid;
    if (x.second->which == CppType::Which::kOptional) {
      dprintf(fd, "  if (it%d_length > %d) {\n", decoder_depth, member_pos);
      dprintf(fd, "    %s%shas_%s = true;\n", name.c_str(),
              member_accessor.c_str(), cid.c_str());
      if (!WriteDecoder(fd, fullname, ".", *x.second->optional_type,
                        decoder_depth, temporary_count)) {
        return false;
      }
      dprintf(fd, "  } else {\n");
      dprintf(fd, "    %s%shas_%s = false;\n", name.c_str(),
              member_accessor.c_str(), cid.c_str());
      dprintf(fd, "  }\n");
    } else {
      if (!WriteDecoder(fd, fullname, ".", *x.second, decoder_depth,
                        temporary_count)) {
        return false;
      }
    }
    ++member_pos;
  }
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it%d, &it%d));\n",
          decoder_depth - 1, decoder_depth);
  return true;
}

// Writes a decoder function definition for every type in |table| to the file
// descriptor |fd|.
bool WriteDecoders(int fd, const CppSymbolTable& table) {
  for (const auto& entry : table.cpp_type_map) {
    int temporary_count = 0;
    const auto& name = entry.first;
    const auto* type = entry.second;
    if (type->which != CppType::Which::kStruct ||
        type->struct_type.key_type == CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    std::string cpp_name = ToCamelCase(name);
    dprintf(fd, "\nssize_t Decode%s(\n", cpp_name.c_str());
    dprintf(fd, "    uint8_t* buffer,\n    size_t length,\n");
    dprintf(fd, "    %s* data) {\n", cpp_name.c_str());
    dprintf(fd, "  CborParser parser;\n");
    dprintf(fd, "  CborValue it0;\n");
    dprintf(
        fd,
        "  CBOR_RETURN_ON_ERROR(cbor_parser_init(buffer, length, 0, &parser, "
        "&it0));\n");
    if (type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
      if (!WriteMapDecoder(fd, "data", "->", type->struct_type.members, 1,
                           &temporary_count)) {
        return false;
      }
    } else {
      if (!WriteArrayDecoder(fd, "data", "->", type->struct_type.members, 1,
                             &temporary_count)) {
        return false;
      }
    }
    dprintf(
        fd,
        "  auto result = static_cast<ssize_t>(cbor_value_get_next_byte(&it0) - "
        "buffer);\n");
    dprintf(fd, "  return result;\n");
    dprintf(fd, "}\n");
  }
  return true;
}

// Converts the filename |header_filename| to a preprocessor token that can be
// used as a header guard macro name.
std::string ToHeaderGuard(const std::string& header_filename) {
  std::string result = header_filename;
  for (auto& c : result) {
    if (c == '/' || c == '.')
      c = '_';
    else
      c = toupper(c);
  }
  result += "_";
  return result;
}

bool WriteHeaderPrologue(int fd, const std::string& header_filename) {
  static const char prologue[] =
      R"(#ifndef %s
#define %s

#include <cstdint>
#include <string>
#include <vector>

namespace openscreen {
namespace msgs {

)";
  std::string header_guard = ToHeaderGuard(header_filename);
  dprintf(fd, prologue, header_guard.c_str(), header_guard.c_str());
  return true;
}

bool WriteHeaderEpilogue(int fd, const std::string& header_filename) {
  static const char epilogue[] = R"(
}  // namespace msgs
}  // namespace openscreen
#endif  // %s
  )";
  std::string header_guard = ToHeaderGuard(header_filename);
  dprintf(fd, epilogue, header_guard.c_str());
  return true;
}

bool WriteSourcePrologue(int fd, const std::string& header_filename) {
  static const char prologue[] =
      R"(#include "%s"

#include "platform/api/logging.h"
#include "third_party/tinycbor/src/src/cbor.h"
#include "third_party/tinycbor/src/src/utf8_p.h"

namespace openscreen {
namespace msgs {
namespace {

#define CBOR_RETURN_WHAT_ON_ERROR(stmt, what)                           \
  {                                                                     \
    CborError error = stmt;                                             \
    /* Encoder-specific errors, so it's fine to check these even in the \
     * parser.                                                          \
     */                                                                 \
    DCHECK_NE(error, CborErrorTooFewItems);                             \
    DCHECK_NE(error, CborErrorTooManyItems);                            \
    DCHECK_NE(error, CborErrorDataTooLarge);                            \
    if (error != CborNoError && error != CborErrorOutOfMemory)          \
      return what;                                                      \
  }
#define CBOR_RETURN_ON_ERROR_INTERNAL(stmt) \
  CBOR_RETURN_WHAT_ON_ERROR(stmt, error)
#define CBOR_RETURN_ON_ERROR(stmt) CBOR_RETURN_WHAT_ON_ERROR(stmt, -error)

#define EXPECT_KEY_CONSTANT(it, key) ExpectKey(it, key, sizeof(key) - 1)

bool IsValidUtf8(const std::string& s) {
  const uint8_t* buffer = reinterpret_cast<const uint8_t*>(s.data());
  const uint8_t* end = buffer + s.size();
  while (buffer < end) {
    // TODO(btolsch): This is an implementation detail of tinycbor so we should
    // eventually replace this call with our own utf8 validation.
    if (get_utf8(&buffer, end) == ~0u)
      return false;
  }
  return true;
}

CborError ExpectKey(CborValue* it, const char* key, size_t key_length) {
  size_t observed_length = 0;
  CBOR_RETURN_ON_ERROR_INTERNAL(
      cbor_value_get_string_length(it, &observed_length));
  if (observed_length != key_length)
    return CborErrorImproperValue;
  std::string observed_key(key_length, 0);
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_copy_text_string(
      it, const_cast<char*>(observed_key.data()), &observed_length, nullptr));
  if (observed_key != key)
    return CborErrorImproperValue;
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_advance(it));
  return CborNoError;
}

}  // namespace
)";
  dprintf(fd, prologue, header_filename.c_str());
  return true;
}

bool WriteSourceEpilogue(int fd) {
  static const char epilogue[] = R"(
}  // namespace msgs
}  // namespace openscreen
)";
  dprintf(fd, epilogue);
  return true;
}
