# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS!="win"', {
      'variables': {
        'config_h_dir':
          '.',  # crafted for gcc/linux.
      },
    }, {  # else, OS=="win"
      'variables': {
        'config_h_dir':
          'src/vsprojects',  # crafted for msvc.
      },
      'target_defaults': {
        'msvs_disabled_warnings': [
          4018,  # signed/unsigned mismatch in comparison
          4244,  # implicit conversion, possible loss of data
          4355,  # 'this' used in base member initializer list
        ],
        'defines!': [
          'WIN32_LEAN_AND_MEAN',  # Protobuf defines this itself.
        ],
      },
    }]
  ],
  'targets': [
    # The "lite" lib is about 1/7th the size of the heavy lib,
    # but it doesn't support some of the more exotic features of
    # protobufs, like reflection.  To generate C++ code that can link
    # against the lite version of the library, add the option line:
    #
    #   option optimize_for = LITE_RUNTIME;
    #
    # to your .proto file.
    {
      'target_name': 'protobuf_lite',
      'type': '<(library)',
      'toolsets': ['host', 'target'],
      'sources': [
        'src/src/google/protobuf/stubs/common.h',
        'src/src/google/protobuf/stubs/once.h',
        'src/src/google/protobuf/extension_set.h',
        'src/src/google/protobuf/generated_message_util.h',
        'src/src/google/protobuf/message_lite.h',
        'src/src/google/protobuf/repeated_field.h',
        'src/src/google/protobuf/wire_format_lite.h',
        'src/src/google/protobuf/wire_format_lite_inl.h',
        'src/src/google/protobuf/io/coded_stream.h',
        'src/src/google/protobuf/io/zero_copy_stream.h',
        'src/src/google/protobuf/io/zero_copy_stream_impl_lite.h',

        'src/src/google/protobuf/stubs/common.cc',
        'src/src/google/protobuf/stubs/once.cc',
        'src/src/google/protobuf/stubs/hash.cc',
        'src/src/google/protobuf/stubs/hash.h',
        'src/src/google/protobuf/stubs/map-util.h',
        'src/src/google/protobuf/stubs/stl_util-inl.h',
        'src/src/google/protobuf/extension_set.cc',
        'src/src/google/protobuf/generated_message_util.cc',
        'src/src/google/protobuf/message_lite.cc',
        'src/src/google/protobuf/repeated_field.cc',
        'src/src/google/protobuf/wire_format_lite.cc',
        'src/src/google/protobuf/io/coded_stream.cc',
        'src/src/google/protobuf/io/zero_copy_stream.cc',
        'src/src/google/protobuf/io/zero_copy_stream_impl_lite.cc',
        '<(config_h_dir)/config.h',
      ],
      'include_dirs': [
        '<(config_h_dir)',
        'src/src',
      ],
      # This macro must be defined to suppress the use of dynamic_cast<>,
      # which requires RTTI.
      'defines': [
        'GOOGLE_PROTOBUF_NO_RTTI',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(config_h_dir)',
          'src/src',
        ],
        'defines': [
          'GOOGLE_PROTOBUF_NO_RTTI',
        ],
      },
    },
    # This is the full, heavy protobuf lib that's needed for c++ .proto's
    # that don't specify the LITE_RUNTIME option.  The protocol
    # compiler itself (protoc) falls into that category.
    {
      'target_name': 'protobuf',
      'type': '<(library)',
      'toolsets': ['host'],
      'sources': [
        'src/src/google/protobuf/descriptor.h',
        'src/src/google/protobuf/descriptor.pb.h',
        'src/src/google/protobuf/descriptor_database.h',
        'src/src/google/protobuf/dynamic_message.h',
        'src/src/google/protobuf/generated_message_reflection.h',
        'src/src/google/protobuf/message.h',
        'src/src/google/protobuf/reflection_ops.h',
        'src/src/google/protobuf/service.h',
        'src/src/google/protobuf/text_format.h',
        'src/src/google/protobuf/unknown_field_set.h',
        'src/src/google/protobuf/wire_format.h',
        'src/src/google/protobuf/wire_format_inl.h',
        'src/src/google/protobuf/io/gzip_stream.h',
        'src/src/google/protobuf/io/printer.h',
        'src/src/google/protobuf/io/tokenizer.h',
        'src/src/google/protobuf/io/zero_copy_stream_impl.h',
        'src/src/google/protobuf/compiler/code_generator.h',
        'src/src/google/protobuf/compiler/command_line_interface.h',
        'src/src/google/protobuf/compiler/importer.h',
        'src/src/google/protobuf/compiler/parser.h',

        'src/src/google/protobuf/stubs/substitute.cc',
        'src/src/google/protobuf/stubs/substitute.h',
        'src/src/google/protobuf/stubs/strutil.cc',
        'src/src/google/protobuf/stubs/strutil.h',
        'src/src/google/protobuf/stubs/structurally_valid.cc',
        'src/src/google/protobuf/descriptor.cc',
        'src/src/google/protobuf/descriptor.pb.cc',
        'src/src/google/protobuf/descriptor_database.cc',
        'src/src/google/protobuf/dynamic_message.cc',
        'src/src/google/protobuf/extension_set_heavy.cc',
        'src/src/google/protobuf/generated_message_reflection.cc',
        'src/src/google/protobuf/message.cc',
        'src/src/google/protobuf/reflection_ops.cc',
        'src/src/google/protobuf/service.cc',
        'src/src/google/protobuf/text_format.cc',
        'src/src/google/protobuf/unknown_field_set.cc',
        'src/src/google/protobuf/wire_format.cc',
        'src/src/google/protobuf/io/gzip_stream.cc',
        'src/src/google/protobuf/io/printer.cc',
        'src/src/google/protobuf/io/tokenizer.cc',
        'src/src/google/protobuf/io/zero_copy_stream_impl.cc',
        'src/src/google/protobuf/io/zero_copy_stream_impl_lite.cc',
        'src/src/google/protobuf/compiler/importer.cc',
        'src/src/google/protobuf/compiler/parser.cc',
      ],
      'dependencies': [
        'protobuf_lite',
      ],
      'export_dependent_settings': [
        'protobuf_lite',
      ],
    },
    {
      'target_name': 'protoc',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'src/src/google/protobuf/compiler/code_generator.cc',
        'src/src/google/protobuf/compiler/command_line_interface.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_enum.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_enum.h',
        'src/src/google/protobuf/compiler/cpp/cpp_enum_field.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_enum_field.h',
        'src/src/google/protobuf/compiler/cpp/cpp_extension.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_extension.h',
        'src/src/google/protobuf/compiler/cpp/cpp_field.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_field.h',
        'src/src/google/protobuf/compiler/cpp/cpp_file.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_file.h',
        'src/src/google/protobuf/compiler/cpp/cpp_generator.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_helpers.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_helpers.h',
        'src/src/google/protobuf/compiler/cpp/cpp_message.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_message.h',
        'src/src/google/protobuf/compiler/cpp/cpp_message_field.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_message_field.h',
        'src/src/google/protobuf/compiler/cpp/cpp_primitive_field.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_primitive_field.h',
        'src/src/google/protobuf/compiler/cpp/cpp_service.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_service.h',
        'src/src/google/protobuf/compiler/cpp/cpp_string_field.cc',
        'src/src/google/protobuf/compiler/cpp/cpp_string_field.h',
        'src/src/google/protobuf/compiler/java/java_enum.cc',
        'src/src/google/protobuf/compiler/java/java_enum.h',
        'src/src/google/protobuf/compiler/java/java_enum_field.cc',
        'src/src/google/protobuf/compiler/java/java_enum_field.h',
        'src/src/google/protobuf/compiler/java/java_extension.cc',
        'src/src/google/protobuf/compiler/java/java_extension.h',
        'src/src/google/protobuf/compiler/java/java_field.cc',
        'src/src/google/protobuf/compiler/java/java_field.h',
        'src/src/google/protobuf/compiler/java/java_file.cc',
        'src/src/google/protobuf/compiler/java/java_file.h',
        'src/src/google/protobuf/compiler/java/java_generator.cc',
        'src/src/google/protobuf/compiler/java/java_helpers.cc',
        'src/src/google/protobuf/compiler/java/java_helpers.h',
        'src/src/google/protobuf/compiler/java/java_message.cc',
        'src/src/google/protobuf/compiler/java/java_message.h',
        'src/src/google/protobuf/compiler/java/java_message_field.cc',
        'src/src/google/protobuf/compiler/java/java_message_field.h',
        'src/src/google/protobuf/compiler/java/java_primitive_field.cc',
        'src/src/google/protobuf/compiler/java/java_primitive_field.h',
        'src/src/google/protobuf/compiler/java/java_service.cc',
        'src/src/google/protobuf/compiler/java/java_service.h',
        'src/src/google/protobuf/compiler/python/python_generator.cc',
        'src/src/google/protobuf/compiler/main.cc',
      ],

      'dependencies': [
        'protobuf',
      ],

      'include_dirs': [
        '<(config_h_dir)',
        'src/src',
      ],
    },
  ],
 }

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
