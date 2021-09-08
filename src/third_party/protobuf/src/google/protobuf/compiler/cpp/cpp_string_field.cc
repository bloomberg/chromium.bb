// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/compiler/cpp/cpp_string_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>


namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

void SetStringVariables(const FieldDescriptor* descriptor,
                        std::map<std::string, std::string>* variables,
                        const Options& options) {
  SetCommonFieldVariables(descriptor, variables, options);
  (*variables)["default"] = DefaultValue(options, descriptor);
  (*variables)["default_length"] =
      StrCat(descriptor->default_value_string().length());
  std::string default_variable_string = MakeDefaultName(descriptor);
  (*variables)["default_variable_name"] = default_variable_string;
  (*variables)["default_variable"] =
      descriptor->default_value_string().empty()
          ? "&::" + (*variables)["proto_ns"] +
                "::internal::GetEmptyStringAlreadyInited()"
          : "&" + QualifiedClassName(descriptor->containing_type(), options) +
                "::" + default_variable_string + ".get()";
  (*variables)["pointer_type"] =
      descriptor->type() == FieldDescriptor::TYPE_BYTES ? "void" : "char";
  (*variables)["null_check"] = (*variables)["DCHK"] + "(value != nullptr);\n";
  // NOTE: Escaped here to unblock proto1->proto2 migration.
  // TODO(liujisi): Extend this to apply for other conflicting methods.
  (*variables)["release_name"] =
      SafeFunctionName(descriptor->containing_type(), descriptor, "release_");
  (*variables)["full_name"] = descriptor->full_name();

  if (options.opensource_runtime) {
    (*variables)["string_piece"] = "::std::string";
  } else {
    (*variables)["string_piece"] = "::StringPiece";
  }

  (*variables)["lite"] =
      HasDescriptorMethods(descriptor->file(), options) ? "" : "Lite";
}

}  // namespace

// ===================================================================

StringFieldGenerator::StringFieldGenerator(const FieldDescriptor* descriptor,
                                           const Options& options)
    : FieldGenerator(descriptor, options),
      lite_(!HasDescriptorMethods(descriptor->file(), options)),
      inlined_(IsStringInlined(descriptor, options)) {
  SetStringVariables(descriptor, &variables_, options);
}

StringFieldGenerator::~StringFieldGenerator() {}

void StringFieldGenerator::GeneratePrivateMembers(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (inlined_) {
    format("::$proto_ns$::internal::InlinedStringField $name$_;\n");
  } else {
    format("::$proto_ns$::internal::ArenaStringPtr $name$_;\n");
  }
}

void StringFieldGenerator::GenerateStaticMembers(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!descriptor_->default_value_string().empty()) {
    // We make the default instance public, so it can be initialized by
    // non-friend code.
    format(
        "public:\n"
        "static ::$proto_ns$::internal::ExplicitlyConstructed<std::string>"
        " $default_variable_name$;\n"
        "private:\n");
  }
}

void StringFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  // If we're using StringFieldGenerator for a field with a ctype, it's
  // because that ctype isn't actually implemented.  In particular, this is
  // true of ctype=CORD and ctype=STRING_PIECE in the open source release.
  // We aren't releasing Cord because it has too many Google-specific
  // dependencies and we aren't releasing StringPiece because it's hardly
  // useful outside of Google and because it would get confusing to have
  // multiple instances of the StringPiece class in different libraries (PCRE
  // already includes it for their C++ bindings, which came from Google).
  //
  // In any case, we make all the accessors private while still actually
  // using a string to represent the field internally.  This way, we can
  // guarantee that if we do ever implement the ctype, it won't break any
  // existing users who might be -- for whatever reason -- already using .proto
  // files that applied the ctype.  The field can still be accessed via the
  // reflection interface since the reflection interface is independent of
  // the string's underlying representation.

  bool unknown_ctype = descriptor_->options().ctype() !=
                       EffectiveStringCType(descriptor_, options_);

  if (unknown_ctype) {
    format.Outdent();
    format(
        " private:\n"
        "  // Hidden due to unknown ctype option.\n");
    format.Indent();
  }

  format(
      "$deprecated_attr$const std::string& ${1$$name$$}$() const;\n"
      "$deprecated_attr$void ${1$set_$name$$}$(const std::string& value);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(std::string&& value);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(const char* value);\n",
      descriptor_);
  if (!options_.opensource_runtime) {
    format(
        "$deprecated_attr$void ${1$set_$name$$}$(::StringPiece value);\n",
        descriptor_);
  }
  format(
      "$deprecated_attr$void ${1$set_$name$$}$(const $pointer_type$* "
      "value, size_t size)"
      ";\n"
      "$deprecated_attr$std::string* ${1$mutable_$name$$}$();\n"
      "$deprecated_attr$std::string* ${1$$release_name$$}$();\n"
      "$deprecated_attr$void ${1$set_allocated_$name$$}$(std::string* "
      "$name$);\n",
      descriptor_);
  format(
      "private:\n"
      "const std::string& _internal_$name$() const;\n"
      "void _internal_set_$name$(const std::string& value);\n"
      "std::string* _internal_mutable_$name$();\n"
      "public:\n");

  if (unknown_ctype) {
    format.Outdent();
    format(" public:\n");
    format.Indent();
  }
}

void StringFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline const std::string& $classname$::$name$() const {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  return _internal_$name$();\n"
      "}\n"
      "inline void $classname$::set_$name$(const std::string& value) {\n"
      "$annotate_accessor$"
      "  _internal_set_$name$(value);\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "}\n"
      "inline std::string* $classname$::mutable_$name$() {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
      "  return _internal_mutable_$name$();\n"
      "}\n"
      "inline const std::string& $classname$::_internal_$name$() const {\n"
      "  return $name$_.Get();\n"
      "}\n"
      "inline void $classname$::_internal_set_$name$(const std::string& "
      "value) {\n"
      "  $set_hasbit$\n"
      "  $name$_.Set$lite$($default_variable$, value, GetArena());\n"
      "}\n"
      "inline void $classname$::set_$name$(std::string&& value) {\n"
      "$annotate_accessor$"
      "  $set_hasbit$\n"
      "  $name$_.Set$lite$(\n"
      "    $default_variable$, ::std::move(value), GetArena());\n"
      "  // @@protoc_insertion_point(field_set_rvalue:$full_name$)\n"
      "}\n"
      "inline void $classname$::set_$name$(const char* value) {\n"
      "$annotate_accessor$"
      "  $null_check$"
      "  $set_hasbit$\n"
      "  $name$_.Set$lite$($default_variable$, $string_piece$(value),\n"
      "              GetArena());\n"
      "  // @@protoc_insertion_point(field_set_char:$full_name$)\n"
      "}\n");
  if (!options_.opensource_runtime) {
    format(
        "inline void $classname$::set_$name$(::StringPiece value) {\n"
        "$annotate_accessor$"
        "  $set_hasbit$\n"
        "  $name$_.Set$lite$($default_variable$, value,GetArena());\n"
        "  // @@protoc_insertion_point(field_set_string_piece:$full_name$)\n"
        "}\n");
  }
  format(
      "inline "
      "void $classname$::set_$name$(const $pointer_type$* value,\n"
      "    size_t size) {\n"
      "$annotate_accessor$"
      "  $set_hasbit$\n"
      "  $name$_.Set$lite$($default_variable$, $string_piece$(\n"
      "      reinterpret_cast<const char*>(value), size), GetArena());\n"
      "  // @@protoc_insertion_point(field_set_pointer:$full_name$)\n"
      "}\n"
      "inline std::string* $classname$::_internal_mutable_$name$() {\n"
      "  $set_hasbit$\n"
      "  return $name$_.Mutable($default_variable$, GetArena());\n"
      "}\n"
      "inline std::string* $classname$::$release_name$() {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_release:$full_name$)\n");

  if (HasHasbit(descriptor_)) {
    format(
        "  if (!_internal_has_$name$()) {\n"
        "    return nullptr;\n"
        "  }\n"
        "  $clear_hasbit$\n"
        "  return $name$_.ReleaseNonDefault("
        "$default_variable$, GetArena());\n");
  } else {
    format("  return $name$_.Release($default_variable$, GetArena());\n");
  }

  format(
      "}\n"
      "inline void $classname$::set_allocated_$name$(std::string* $name$) {\n"
      "$annotate_accessor$"
      "  if ($name$ != nullptr) {\n"
      "    $set_hasbit$\n"
      "  } else {\n"
      "    $clear_hasbit$\n"
      "  }\n"
      "  $name$_.SetAllocated($default_variable$, $name$,\n"
      "      GetArena());\n"
      "  // @@protoc_insertion_point(field_set_allocated:$full_name$)\n"
      "}\n");
}

void StringFieldGenerator::GenerateNonInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!descriptor_->default_value_string().empty()) {
    // Initialized in GenerateDefaultInstanceAllocator.
    format(
        "::$proto_ns$::internal::ExplicitlyConstructed<std::string> "
        "$classname$::$default_variable_name$;\n");
  }
}

void StringFieldGenerator::GenerateClearingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  // Two-dimension specialization here: supporting arenas or not, and default
  // value is the empty string or not. Complexity here ensures the minimal
  // number of branches / amount of extraneous code at runtime (given that the
  // below methods are inlined one-liners)!
  if (descriptor_->default_value_string().empty()) {
    format("$name$_.ClearToEmpty($default_variable$, GetArena());\n");
  } else {
    format("$name$_.ClearToDefault($default_variable$, GetArena());\n");
  }
}

void StringFieldGenerator::GenerateMessageClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  // Two-dimension specialization here: supporting arenas, field presence, or
  // not, and default value is the empty string or not. Complexity here ensures
  // the minimal number of branches / amount of extraneous code at runtime
  // (given that the below methods are inlined one-liners)!

  // If we have a hasbit, then the Clear() method of the protocol buffer
  // will have checked that this field is set.  If so, we can avoid redundant
  // checks against default_variable.
  const bool must_be_present = HasHasbit(descriptor_);

  if (inlined_ && must_be_present) {
    // Calling mutable_$name$() gives us a string reference and sets the has bit
    // for $name$ (in proto2).  We may get here when the string field is inlined
    // but the string's contents have not been changed by the user, so we cannot
    // make an assertion about the contents of the string and could never make
    // an assertion about the string instance.
    //
    // For non-inlined strings, we distinguish from non-default by comparing
    // instances, rather than contents.
    format("$DCHK$(!$name$_.IsDefault($default_variable$));\n");
  }

  if (descriptor_->default_value_string().empty()) {
    if (must_be_present) {
      format("$name$_.ClearNonDefaultToEmpty();\n");
    } else {
      format("$name$_.ClearToEmpty($default_variable$, GetArena());\n");
    }
  } else {
    // Clear to a non-empty default is more involved, as we try to use the
    // Arena if one is present and may need to reallocate the string.
    format("$name$_.ClearToDefault($default_variable$, GetArena());\n");
  }
}

void StringFieldGenerator::GenerateMergingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  // TODO(gpike): improve this
  format("_internal_set_$name$(from._internal_$name$());\n");
}

void StringFieldGenerator::GenerateSwappingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (inlined_) {
    format("$name$_.Swap(&other->$name$_);\n");
  } else {
    format("$name$_.Swap(&other->$name$_, $default_variable$, GetArena());\n");
  }
}

void StringFieldGenerator::GenerateConstructorCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  // TODO(ckennelly): Construct non-empty strings as part of the initializer
  // list.
  if (inlined_ && descriptor_->default_value_string().empty()) {
    // Automatic initialization will construct the string.
    return;
  }

  format("$name$_.UnsafeSetDefault($default_variable$);\n");
}

void StringFieldGenerator::GenerateCopyConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  GenerateConstructorCode(printer);

  if (HasHasbit(descriptor_)) {
    format("if (from._internal_has_$name$()) {\n");
  } else {
    format("if (!from._internal_$name$().empty()) {\n");
  }

  format.Indent();

  // TODO(gpike): improve this
  format(
      "$name$_.Set$lite$($default_variable$, from._internal_$name$(),\n"
      "  GetArena());\n");

  format.Outdent();
  format("}\n");
}

void StringFieldGenerator::GenerateDestructorCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (inlined_) {
    // The destructor is automatically invoked.
    return;
  }

  format("$name$_.DestroyNoArena($default_variable$);\n");
}

bool StringFieldGenerator::GenerateArenaDestructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!inlined_) {
    return false;
  }

  format("_this->$name$_.DestroyNoArena($default_variable$);\n");
  return true;
}

void StringFieldGenerator::GenerateDefaultInstanceAllocator(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!descriptor_->default_value_string().empty()) {
    format(
        "$ns$::$classname$::$default_variable_name$.DefaultConstruct();\n"
        "*$ns$::$classname$::$default_variable_name$.get_mutable() = "
        "std::string($default$, $default_length$);\n"
        "::$proto_ns$::internal::OnShutdownDestroyString(\n"
        "    $ns$::$classname$::$default_variable_name$.get_mutable());\n");
  }
}

bool StringFieldGenerator::MergeFromCodedStreamNeedsArena() const {
  return !lite_ && !inlined_ && !options_.opensource_runtime;
}

void StringFieldGenerator::GenerateSerializeWithCachedSizesToArray(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (descriptor_->type() == FieldDescriptor::TYPE_STRING) {
    GenerateUtf8CheckCodeForString(
        descriptor_, options_, false,
        "this->_internal_$name$().data(), "
        "static_cast<int>(this->_internal_$name$().length()),\n",
        format);
  }
  format(
      "target = stream->Write$declared_type$MaybeAliased(\n"
      "    $number$, this->_internal_$name$(), target);\n");
}

void StringFieldGenerator::GenerateByteSize(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "total_size += $tag_size$ +\n"
      "  ::$proto_ns$::internal::WireFormatLite::$declared_type$Size(\n"
      "    this->_internal_$name$());\n");
}

uint32 StringFieldGenerator::CalculateFieldTag() const {
  return inlined_ ? 1 : 0;
}

// ===================================================================

StringOneofFieldGenerator::StringOneofFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : StringFieldGenerator(descriptor, options) {
  inlined_ = false;

  SetCommonOneofFieldVariables(descriptor, &variables_);
  variables_["field_name"] = UnderscoresToCamelCase(descriptor->name(), true);
  variables_["oneof_index"] =
      StrCat(descriptor->containing_oneof()->index());
}

StringOneofFieldGenerator::~StringOneofFieldGenerator() {}

void StringOneofFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline const std::string& $classname$::$name$() const {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  return _internal_$name$();\n"
      "}\n"
      "inline void $classname$::set_$name$(const std::string& value) {\n"
      "$annotate_accessor$"
      "  _internal_set_$name$(value);\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "}\n"
      "inline std::string* $classname$::mutable_$name$() {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
      "  return _internal_mutable_$name$();\n"
      "}\n"
      "inline const std::string& $classname$::_internal_$name$() const {\n"
      "  if (_internal_has_$name$()) {\n"
      "    return $field_member$.Get();\n"
      "  }\n"
      "  return *$default_variable$;\n"
      "}\n"
      "inline void $classname$::_internal_set_$name$(const std::string& "
      "value) {\n"
      "  if (!_internal_has_$name$()) {\n"
      "    clear_$oneof_name$();\n"
      "    set_has_$name$();\n"
      "    $field_member$.UnsafeSetDefault($default_variable$);\n"
      "  }\n"
      "  $field_member$.Set$lite$($default_variable$, value, GetArena());\n"
      "}\n"
      "inline void $classname$::set_$name$(std::string&& value) {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "  if (!_internal_has_$name$()) {\n"
      "    clear_$oneof_name$();\n"
      "    set_has_$name$();\n"
      "    $field_member$.UnsafeSetDefault($default_variable$);\n"
      "  }\n"
      "  $field_member$.Set$lite$(\n"
      "    $default_variable$, ::std::move(value), GetArena());\n"
      "  // @@protoc_insertion_point(field_set_rvalue:$full_name$)\n"
      "}\n"
      "inline void $classname$::set_$name$(const char* value) {\n"
      "$annotate_accessor$"
      "  $null_check$"
      "  if (!_internal_has_$name$()) {\n"
      "    clear_$oneof_name$();\n"
      "    set_has_$name$();\n"
      "    $field_member$.UnsafeSetDefault($default_variable$);\n"
      "  }\n"
      "  $field_member$.Set$lite$($default_variable$,\n"
      "      $string_piece$(value), GetArena());\n"
      "  // @@protoc_insertion_point(field_set_char:$full_name$)\n"
      "}\n");
  if (!options_.opensource_runtime) {
    format(
        "inline void $classname$::set_$name$(::StringPiece value) {\n"
        "$annotate_accessor$"
        "  if (!_internal_has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  $field_member$.Set$lite$($default_variable$, value,\n"
        "      GetArena());\n"
        "  // @@protoc_insertion_point(field_set_string_piece:$full_name$)\n"
        "}\n");
  }
  format(
      "inline "
      "void $classname$::set_$name$(const $pointer_type$* value,\n"
      "                             size_t size) {\n"
      "$annotate_accessor$"
      "  if (!_internal_has_$name$()) {\n"
      "    clear_$oneof_name$();\n"
      "    set_has_$name$();\n"
      "    $field_member$.UnsafeSetDefault($default_variable$);\n"
      "  }\n"
      "  $field_member$.Set$lite$(\n"
      "      $default_variable$, $string_piece$(\n"
      "      reinterpret_cast<const char*>(value), size),\n"
      "      GetArena());\n"
      "  // @@protoc_insertion_point(field_set_pointer:$full_name$)\n"
      "}\n"
      "inline std::string* $classname$::_internal_mutable_$name$() {\n"
      "  if (!_internal_has_$name$()) {\n"
      "    clear_$oneof_name$();\n"
      "    set_has_$name$();\n"
      "    $field_member$.UnsafeSetDefault($default_variable$);\n"
      "  }\n"
      "  return $field_member$.Mutable($default_variable$, GetArena());\n"
      "}\n"
      "inline std::string* $classname$::$release_name$() {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_release:$full_name$)\n"
      "  if (_internal_has_$name$()) {\n"
      "    clear_has_$oneof_name$();\n"
      "    return $field_member$.Release($default_variable$, GetArena());\n"
      "  } else {\n"
      "    return nullptr;\n"
      "  }\n"
      "}\n"
      "inline void $classname$::set_allocated_$name$(std::string* $name$) {\n"
      "$annotate_accessor$"
      "  if (has_$oneof_name$()) {\n"
      "    clear_$oneof_name$();\n"
      "  }\n"
      "  if ($name$ != nullptr) {\n"
      "    set_has_$name$();\n"
      "    $field_member$.UnsafeSetDefault($name$);\n"
      "    ::$proto_ns$::Arena* arena = GetArena();\n"
      "    if (arena != nullptr) {\n"
      "      arena->Own($name$);\n"
      "    }\n"
      "  }\n"
      "  // @@protoc_insertion_point(field_set_allocated:$full_name$)\n"
      "}\n");
}

void StringOneofFieldGenerator::GenerateClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$field_member$.Destroy($default_variable$, GetArena());\n");
}

void StringOneofFieldGenerator::GenerateMessageClearingCode(
    io::Printer* printer) const {
  return GenerateClearingCode(printer);
}

void StringOneofFieldGenerator::GenerateSwappingCode(
    io::Printer* printer) const {
  // Don't print any swapping code. Swapping the union will swap this field.
}

void StringOneofFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "$ns$::_$classname$_default_instance_.$name$_.UnsafeSetDefault(\n"
      "    $default_variable$);\n");
}

// ===================================================================

RepeatedStringFieldGenerator::RepeatedStringFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : FieldGenerator(descriptor, options) {
  SetStringVariables(descriptor, &variables_, options);
}

RepeatedStringFieldGenerator::~RepeatedStringFieldGenerator() {}

void RepeatedStringFieldGenerator::GeneratePrivateMembers(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("::$proto_ns$::RepeatedPtrField<std::string> $name$_;\n");
}

void RepeatedStringFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  // See comment above about unknown ctypes.
  bool unknown_ctype = descriptor_->options().ctype() !=
                       EffectiveStringCType(descriptor_, options_);

  if (unknown_ctype) {
    format.Outdent();
    format(
        " private:\n"
        "  // Hidden due to unknown ctype option.\n");
    format.Indent();
  }

  format(
      "$deprecated_attr$const std::string& ${1$$name$$}$(int index) const;\n"
      "$deprecated_attr$std::string* ${1$mutable_$name$$}$(int index);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(int index, const "
      "std::string& value);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(int index, std::string&& "
      "value);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(int index, const "
      "char* value);\n",
      descriptor_);
  if (!options_.opensource_runtime) {
    format(
        "$deprecated_attr$void ${1$set_$name$$}$(int index, "
        "StringPiece value);\n",
        descriptor_);
  }
  format(
      "$deprecated_attr$void ${1$set_$name$$}$("
      "int index, const $pointer_type$* value, size_t size);\n"
      "$deprecated_attr$std::string* ${1$add_$name$$}$();\n"
      "$deprecated_attr$void ${1$add_$name$$}$(const std::string& value);\n"
      "$deprecated_attr$void ${1$add_$name$$}$(std::string&& value);\n"
      "$deprecated_attr$void ${1$add_$name$$}$(const char* value);\n",
      descriptor_);
  if (!options_.opensource_runtime) {
    format(
        "$deprecated_attr$void ${1$add_$name$$}$(StringPiece value);\n",
        descriptor_);
  }
  format(
      "$deprecated_attr$void ${1$add_$name$$}$(const $pointer_type$* "
      "value, size_t size)"
      ";\n"
      "$deprecated_attr$const ::$proto_ns$::RepeatedPtrField<std::string>& "
      "${1$$name$$}$() "
      "const;\n"
      "$deprecated_attr$::$proto_ns$::RepeatedPtrField<std::string>* "
      "${1$mutable_$name$$}$()"
      ";\n"
      "private:\n"
      "const std::string& ${1$_internal_$name$$}$(int index) const;\n"
      "std::string* _internal_add_$name$();\n"
      "public:\n",
      descriptor_);

  if (unknown_ctype) {
    format.Outdent();
    format(" public:\n");
    format.Indent();
  }
}

void RepeatedStringFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline std::string* $classname$::add_$name$() {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_add_mutable:$full_name$)\n"
      "  return _internal_add_$name$();\n"
      "}\n");
  if (options_.safe_boundary_check) {
    format(
        "inline const std::string& $classname$::_internal_$name$(int index) "
        "const {\n"
        "  return $name$_.InternalCheckedGet(\n"
        "      index, ::$proto_ns$::internal::GetEmptyStringAlreadyInited());\n"
        "}\n");
  } else {
    format(
        "inline const std::string& $classname$::_internal_$name$(int index) "
        "const {\n"
        "  return $name$_.Get(index);\n"
        "}\n");
  }
  format(
      "inline const std::string& $classname$::$name$(int index) const {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  return _internal_$name$(index);\n"
      "}\n"
      "inline std::string* $classname$::mutable_$name$(int index) {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
      "  return $name$_.Mutable(index);\n"
      "}\n"
      "inline void $classname$::set_$name$(int index, const std::string& "
      "value) "
      "{\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "  $name$_.Mutable(index)->assign(value);\n"
      "}\n"
      "inline void $classname$::set_$name$(int index, std::string&& value) {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "  $name$_.Mutable(index)->assign(std::move(value));\n"
      "}\n"
      "inline void $classname$::set_$name$(int index, const char* value) {\n"
      "$annotate_accessor$"
      "  $null_check$"
      "  $name$_.Mutable(index)->assign(value);\n"
      "  // @@protoc_insertion_point(field_set_char:$full_name$)\n"
      "}\n");
  if (!options_.opensource_runtime) {
    format(
        "inline void "
        "$classname$::set_$name$(int index, StringPiece value) {\n"
        "$annotate_accessor$"
        "  $name$_.Mutable(index)->assign(value.data(), value.size());\n"
        "  // @@protoc_insertion_point(field_set_string_piece:$full_name$)\n"
        "}\n");
  }
  format(
      "inline void "
      "$classname$::set_$name$"
      "(int index, const $pointer_type$* value, size_t size) {\n"
      "$annotate_accessor$"
      "  $name$_.Mutable(index)->assign(\n"
      "    reinterpret_cast<const char*>(value), size);\n"
      "  // @@protoc_insertion_point(field_set_pointer:$full_name$)\n"
      "}\n"
      "inline std::string* $classname$::_internal_add_$name$() {\n"
      "  return $name$_.Add();\n"
      "}\n"
      "inline void $classname$::add_$name$(const std::string& value) {\n"
      "$annotate_accessor$"
      "  $name$_.Add()->assign(value);\n"
      "  // @@protoc_insertion_point(field_add:$full_name$)\n"
      "}\n"
      "inline void $classname$::add_$name$(std::string&& value) {\n"
      "$annotate_accessor$"
      "  $name$_.Add(std::move(value));\n"
      "  // @@protoc_insertion_point(field_add:$full_name$)\n"
      "}\n"
      "inline void $classname$::add_$name$(const char* value) {\n"
      "$annotate_accessor$"
      "  $null_check$"
      "  $name$_.Add()->assign(value);\n"
      "  // @@protoc_insertion_point(field_add_char:$full_name$)\n"
      "}\n");
  if (!options_.opensource_runtime) {
    format(
        "inline void $classname$::add_$name$(StringPiece value) {\n"
        "$annotate_accessor$"
        "  $name$_.Add()->assign(value.data(), value.size());\n"
        "  // @@protoc_insertion_point(field_add_string_piece:$full_name$)\n"
        "}\n");
  }
  format(
      "inline void "
      "$classname$::add_$name$(const $pointer_type$* value, size_t size) {\n"
      "$annotate_accessor$"
      "  $name$_.Add()->assign(reinterpret_cast<const char*>(value), size);\n"
      "  // @@protoc_insertion_point(field_add_pointer:$full_name$)\n"
      "}\n"
      "inline const ::$proto_ns$::RepeatedPtrField<std::string>&\n"
      "$classname$::$name$() const {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_list:$full_name$)\n"
      "  return $name$_;\n"
      "}\n"
      "inline ::$proto_ns$::RepeatedPtrField<std::string>*\n"
      "$classname$::mutable_$name$() {\n"
      "$annotate_accessor$"
      "  // @@protoc_insertion_point(field_mutable_list:$full_name$)\n"
      "  return &$name$_;\n"
      "}\n");
}

void RepeatedStringFieldGenerator::GenerateClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.Clear();\n");
}

void RepeatedStringFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.MergeFrom(from.$name$_);\n");
}

void RepeatedStringFieldGenerator::GenerateSwappingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.InternalSwap(&other->$name$_);\n");
}

void RepeatedStringFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  // Not needed for repeated fields.
}

void RepeatedStringFieldGenerator::GenerateCopyConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.CopyFrom(from.$name$_);");
}

void RepeatedStringFieldGenerator::GenerateSerializeWithCachedSizesToArray(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "for (int i = 0, n = this->_internal_$name$_size(); i < n; i++) {\n"
      "  const auto& s = this->_internal_$name$(i);\n");
  // format("for (const std::string& s : this->$name$()) {\n");
  format.Indent();
  if (descriptor_->type() == FieldDescriptor::TYPE_STRING) {
    GenerateUtf8CheckCodeForString(descriptor_, options_, false,
                                   "s.data(), static_cast<int>(s.length()),\n",
                                   format);
  }
  format.Outdent();
  format(
      "  target = stream->Write$declared_type$($number$, s, target);\n"
      "}\n");
}

void RepeatedStringFieldGenerator::GenerateByteSize(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "total_size += $tag_size$ *\n"
      "    ::$proto_ns$::internal::FromIntSize($name$_.size());\n"
      "for (int i = 0, n = $name$_.size(); i < n; i++) {\n"
      "  total_size += "
      "::$proto_ns$::internal::WireFormatLite::$declared_type$Size(\n"
      "    $name$_.Get(i));\n"
      "}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
