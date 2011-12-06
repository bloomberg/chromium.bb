# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'conditions': [
    ['target_arch=="x64"', {
      'variables': {
        # No extra reservation.
        'nacl_reserve_top': [],
      }
    }],
    ['target_arch=="ia32"', {
      'variables': {
        # 1G address space.
        'nacl_reserve_top': ['--defsym', 'RESERVE_TOP=0x40000000'],
      }
    }],
    ['target_arch=="arm"', {
      'variables': {
        # 1G address space, plus 8K guard area above.
        'nacl_reserve_top': ['--defsym', 'RESERVE_TOP=0x40002000'],
      }
    }],
  ],
  'targets': [
    {
      'target_name': 'nacl_bootstrap_munge_phdr',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'nacl_bootstrap_munge_phdr.c',
      ],
      'libraries': [
        '-lelf',
      ],
      # This is an ugly kludge because gyp doesn't actually treat
      # host_arch=x64 target_arch=ia32 as proper cross compilation.
      # It still wants to compile the "host" program with -m32 et
      # al.  Though a program built that way can indeed run on the
      # x86-64 host, we cannot reliably build this program on such a
      # host because Ubuntu does not provide the full suite of
      # x86-32 libraries in packages that can be installed on an
      # x86-64 host; in particular, libelf is missing.  So here we
      # use the hack of eliding all the -m* flags from the
      # compilation lines, getting the command close to what they
      # would be if gyp were to really build properly for the host.
      # TODO(bradnelson): Clean up with proper cross support.
      'cflags/': [['exclude', '-m.*'],
                  ['exclude', '--sysroot=.*']],
      'ldflags/': [['exclude', '-m.*'],
                   ['exclude', '--sysroot=.*']],
    },
    {
      'target_name': 'nacl_bootstrap_lib',
      'type': 'static_library',
      'product_dir': '<(SHARED_INTERMEDIATE_DIR)/nacl_bootstrap',
      'hard_depencency': 1,
      'include_dirs': [
        '..',
      ],
      'sources': [
        'nacl_bootstrap.c',
      ],
      'cflags': [
        # The tiny standalone bootstrap program is incompatible with
        # -fstack-protector, which might be on by default.  That switch
        # requires using the standard libc startup code, which we do not.
        '-fno-stack-protector',
        # We don't want to compile it PIC (or its cousin PIE), because
        # it goes at an absolute address anyway, and because any kind
        # of PIC complicates life for the x86-32 assembly code.  We
        # append -fno-* flags here instead of using a 'cflags!' stanza
        # to remove -f* flags, just in case some system's compiler
        # defaults to using PIC for everything.
        '-fno-pic', '-fno-PIC',
        '-fno-pie', '-fno-PIE',
      ],
      'cflags!': [
        # TODO(glider): -fasan is deprecated.
        '-fasan',
        '-faddress-sanitizer',
        '-w',
      ],
      'conditions': [
        ['clang==1', {
          'cflags': [
            # Prevent llvm-opt from replacing my_bzero with a call
            # to memset
            '-ffreestanding',
            # But make its <limits.h> still work!
            '-U__STDC_HOSTED__', '-D__STDC_HOSTED__=1',
          ],
        }],
      ],
    },
    {
      'target_name': 'nacl_bootstrap_raw',
      'type': 'none',
      'dependencies': [
        'nacl_bootstrap_lib',
      ],
      'actions': [
        {
          'action_name': 'link_with_ld_bfd',
          'variables': {
            'bootstrap_lib': '<(SHARED_INTERMEDIATE_DIR)/nacl_bootstrap/<(STATIC_LIB_PREFIX)nacl_bootstrap_lib<(STATIC_LIB_SUFFIX)',
            'linker_script': 'nacl_bootstrap.x',
          },
          'inputs': [
            '<(linker_script)',
            '<(bootstrap_lib)',
            'ld_bfd.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/nacl_bootstrap_raw',
          ],
          'message': 'Linking nacl_bootstrap_raw',
          'conditions': [
            ['target_arch=="x64"', {
              'variables': {
                'linker_emulation': 'elf_x86_64',
              }
            }],
            ['target_arch=="ia32"', {
              'variables': {
                'linker_emulation': 'elf_i386',
              }
            }],
            ['target_arch=="arm"', {
              'variables': {
                'linker_emulation': 'armelf_linux_eabi',
              }
            }],
          ],
          'action': ['python', 'ld_bfd.py',
                     '-m', '<(linker_emulation)',
                     '--build-id',
                     # This program is (almost) entirely
                     # standalone.  It has its own startup code, so
                     # no crt1.o for it.  It is statically linked,
                     # and on x86 it does not use libc at all.
                     # However, on ARM it needs a few (safe) things
                     # from libc.
                     '-static',
                     # Link with custom linker script for special
                     # layout.  The script uses the symbol RESERVE_TOP.
                     '<@(nacl_reserve_top)',
                     '--script=<(linker_script)',
                     '-o', '<@(_outputs)',
                     # On x86-64, the default page size with some
                     # linkers is 2M rather than the real Linux page
                     # size of 4K.  A larger page size is incompatible
                     # with our custom linker script's special layout.
                     '-z', 'max-page-size=0x1000',
                     '--whole-archive', '<(bootstrap_lib)',
                     '--no-whole-archive',
                   ],
        }
      ],
    },
    {
      'target_name': 'nacl_helper_bootstrap',
      'dependencies': [
        'nacl_bootstrap_raw',
        'nacl_bootstrap_munge_phdr#host',
      ],
      'type': 'none',
      'actions': [{
        'action_name': 'munge_phdr',
        'inputs': ['nacl_bootstrap_munge_phdr.py',
                   '<(PRODUCT_DIR)/nacl_bootstrap_munge_phdr',
                   '<(PRODUCT_DIR)/nacl_bootstrap_raw'],
        'outputs': ['<(PRODUCT_DIR)/nacl_helper_bootstrap'],
        'message': 'Munging ELF program header',
        'action': ['python', '<@(_inputs)', '<@(_outputs)']
      }],
    },
  ],
}
