# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
      'common_sources': [
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
        'src/src/google/protobuf/io/coded_stream_inl.h',
        'src/src/google/protobuf/io/zero_copy_stream.cc',
        'src/src/google/protobuf/io/zero_copy_stream_impl_lite.cc',
        '<(config_h_dir)/config.h',
      ],
    },
    'target_conditions': [
      ['target_base=="protobuf_lite"', {
        'sources': [
          '<@(common_sources)',
        ],
      }],
      ['target_base=="protobuf"', {
        'sources': [
          '<@(common_sources)',
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
          'src/src/google/protobuf/stubs/strutil.cc',
          'src/src/google/protobuf/stubs/strutil.h',
          'src/src/google/protobuf/stubs/substitute.cc',
          'src/src/google/protobuf/stubs/substitute.h',
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
          # This file pulls in zlib, but it's not actually used by protoc, so
          # instead of compiling zlib for the host, let's just exclude this.
          # 'src/src/google/protobuf/io/gzip_stream.cc',
          'src/src/google/protobuf/io/printer.cc',
          'src/src/google/protobuf/io/tokenizer.cc',
          'src/src/google/protobuf/io/zero_copy_stream_impl.cc',
          'src/src/google/protobuf/compiler/importer.cc',
          'src/src/google/protobuf/compiler/parser.cc',
        ],
      }],
    ],
    # This macro must be defined to suppress the use of dynamic_cast<>,
    # which requires RTTI.
    'defines': [
      'GOOGLE_PROTOBUF_NO_RTTI',
    ],
    'include_dirs': [
      '<(config_h_dir)',
      'src/src',
    ],
    'conditions': [
      ['OS=="win"', {
        'msvs_disabled_warnings': [
          4018,  # signed/unsigned mismatch in comparison
          4146,  # unary minus operator applied to unsigned
          4244,  # implicit conversion, possible loss of data
          4267,  # conversion loses precision
          4355,  # 'this' used in base member initializer list
          4800,  # forcing value to bool 'true' or 'false'
          4996,  # deprecated/unsafe function
        ],
        'defines!': [
          'WIN32_LEAN_AND_MEAN',  # Protobuf defines this itself.
        ],
      }],
      ['OS=="linux"', {
        'cflags!': [
          '-Werror',
          '-pedantic',
          '-Wsign-compare',
          '-Wswitch-enum',
        ],
        'cflags': [
          '-Wno-deprecated',
          '-Wno-sign-compare',
          '-Wno-unused-parameter',
        ],
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',
          'WARNING_CFLAGS!': [
            '-Werror',
            '-pedantic',
            '-Wswitch-enum',
            '-Wsign-compare',
          ],
          'WARNING_CFLAGS': [
            '-Wno-deprecated',
            '-Wno-sign-compare',
            '-Wno-unused-parameter',
          ],
        },
      }],
    ],
  },
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
      'type': 'static_library',
      'variables': {
        'target_base': 'protobuf_lite',
      },
    },
    # This is the full, heavy protobuf lib that's needed for c++ .proto's
    # that don't specify the LITE_RUNTIME option.  The protocol
    # compiler itself (protoc) falls into that category.
    {
      'target_name': 'protobuf',
      'type': 'static_library',
      'variables': {
        'target_base': 'protobuf',
      },
    },
    {
      'target_name': 'protoc',
      'type': 'executable',
      'sources': [
        'src/src/google/protobuf/compiler/code_generator.cc',
        'src/src/google/protobuf/compiler/command_line_interface.cc',
        'src/src/google/protobuf/compiler/plugin.cc',
        'src/src/google/protobuf/compiler/plugin.pb.cc',
        'src/src/google/protobuf/compiler/subprocess.cc',
        'src/src/google/protobuf/compiler/subprocess.h',
        'src/src/google/protobuf/compiler/zip_writer.cc',
        'src/src/google/protobuf/compiler/zip_writer.h',
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
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'variables': {
        'config_h_dir':
          'src/vsprojects',  # crafted for msvc.
      },
      'targets': [
        {
          'target_name': 'protobuf_lite64',
          'type': 'static_library',
          'variables': {
            'target_base': 'protobuf_lite',
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            }
          },
        },
        {
          'target_name': 'protobuf64',
          'type': 'static_library',
          'variables': {
            'target_base': 'protobuf',
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            }
          },
        }
      ],
    }, {
      'variables': {
        'config_h_dir':
          '.',  # crafted for gcc/linux.
      },
    }],
  ],
}
