# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
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
    }]
  ],
  'targets': [
    { 'target_name': 'protobuf',
      'type': '<(library)',
      'sources': [
        'src/src/google/protobuf/stubs/common.h',
        'src/src/google/protobuf/stubs/once.h',
        'src/src/google/protobuf/descriptor.h',
        'src/src/google/protobuf/descriptor.pb.h',
        'src/src/google/protobuf/descriptor_database.h',
        'src/src/google/protobuf/dynamic_message.h',
        'src/src/google/protobuf/extension_set.h',
        'src/src/google/protobuf/generated_message_reflection.h',
        'src/src/google/protobuf/message.h',
        'src/src/google/protobuf/reflection_ops.h',
        'src/src/google/protobuf/repeated_field.h',
        'src/src/google/protobuf/service.h',
        'src/src/google/protobuf/text_format.h',
        'src/src/google/protobuf/unknown_field_set.h',
        'src/src/google/protobuf/wire_format.h',
        'src/src/google/protobuf/wire_format_inl.h',
        'src/src/google/protobuf/io/coded_stream.h',
        'src/src/google/protobuf/io/gzip_stream.h',
        'src/src/google/protobuf/io/printer.h',
        'src/src/google/protobuf/io/tokenizer.h',
        'src/src/google/protobuf/io/zero_copy_stream.h',
        'src/src/google/protobuf/io/zero_copy_stream_impl.h',
        'src/src/google/protobuf/compiler/code_generator.h',
        'src/src/google/protobuf/compiler/command_line_interface.h',
        'src/src/google/protobuf/compiler/importer.h',
        'src/src/google/protobuf/compiler/parser.h',

        'src/src/google/protobuf/stubs/common.cc',
        'src/src/google/protobuf/stubs/once.cc',
        'src/src/google/protobuf/stubs/hash.cc',
        'src/src/google/protobuf/stubs/hash.h',
        'src/src/google/protobuf/stubs/map-util.h',
        'src/src/google/protobuf/stubs/stl_util-inl.h',
        'src/src/google/protobuf/stubs/substitute.cc',
        'src/src/google/protobuf/stubs/substitute.h',
        'src/src/google/protobuf/stubs/strutil.cc',
        'src/src/google/protobuf/stubs/strutil.h',
        'src/src/google/protobuf/stubs/structurally_valid.cc',
        'src/src/google/protobuf/descriptor.cc',
        'src/src/google/protobuf/descriptor.pb.cc',
        'src/src/google/protobuf/descriptor_database.cc',
        'src/src/google/protobuf/dynamic_message.cc',
        'src/src/google/protobuf/extension_set.cc',
        'src/src/google/protobuf/extension_set_heavy.cc',
        'src/src/google/protobuf/generated_message_reflection.cc',
        'src/src/google/protobuf/message.cc',
        'src/src/google/protobuf/message_lite.cc',
        'src/src/google/protobuf/reflection_ops.cc',
        'src/src/google/protobuf/repeated_field.cc',
        'src/src/google/protobuf/service.cc',
        'src/src/google/protobuf/text_format.cc',
        'src/src/google/protobuf/unknown_field_set.cc',
        'src/src/google/protobuf/wire_format.cc',
        'src/src/google/protobuf/wire_format_lite.cc',
        'src/src/google/protobuf/io/coded_stream.cc',
        'src/src/google/protobuf/io/gzip_stream.cc',
        'src/src/google/protobuf/io/printer.cc',
        'src/src/google/protobuf/io/tokenizer.cc',
        'src/src/google/protobuf/io/zero_copy_stream.cc',
        'src/src/google/protobuf/io/zero_copy_stream_impl.cc',
        'src/src/google/protobuf/io/zero_copy_stream_impl_lite.cc',
        'src/src/google/protobuf/compiler/importer.cc',
        'src/src/google/protobuf/compiler/parser.cc',
        '<(config_h_dir)/config.h',
      ],

      'conditions': [
        ['OS != "win"', {
          # src/src/google/protobuf/generated_message_reflection.h can figure
          # out whether RTTI is enabled or disabled via compiler-defined macros
          # when building with MSVC.  For other compilers, this macro must be
          # defined to suppress the use of dynamic_cast<>, which requires RTTI.
          'defines': [
            'GOOGLE_PROTOBUF_NO_RTTI',
          ],
          'direct_dependent_settings': {
            'defines': [
              'GOOGLE_PROTOBUF_NO_RTTI',
            ],
          },
        }],
      ],

      'include_dirs': [
        '<(config_h_dir)',
        'src/src',
      ],

      'direct_dependent_settings': {
        'include_dirs': [
          '<(config_h_dir)',
          'src/src',
        ],
      },
    },
    { 'target_name': 'protoc',
      'type': 'executable',
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
