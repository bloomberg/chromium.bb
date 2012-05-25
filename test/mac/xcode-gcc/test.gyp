# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'xcode_settings': {
      'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',
    },
  },

  # Targets come in pairs: 'foo' and 'foo-fail', with the former building with
  # no warnings and the latter not.
  'targets': [
    # GCC_WARN_ABOUT_INVALID_OFFSETOF_MACRO (default: YES):
    {
      'target_name': 'warn_about_invalid_offsetof_macro',
      'type': 'executable',
      'sources': [ 'warn_about_invalid_offsetof_macro.cc', ],
      'xcode_settings': {
        'GCC_WARN_ABOUT_INVALID_OFFSETOF_MACRO': 'NO',
      },
    },
    {
      'target_name': 'warn_about_invalid_offsetof_macro-fail',
      'type': 'executable',
      'sources': [ 'warn_about_invalid_offsetof_macro.cc', ],
    },
    # GCC_WARN_ABOUT_MISSING_NEWLINE (default: NO):
    {
      'target_name': 'warn_about_missing_newline',
      'type': 'executable',
      'sources': [ 'warn_about_missing_newline.c', ],
    },
    {
      'target_name': 'warn_about_missing_newline-fail',
      'type': 'executable',
      'sources': [ 'warn_about_missing_newline.c', ],
      'xcode_settings': {
        'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',
      },
    },
  ],
}
