# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(ajwong): Determine if we want to statically link libz.

{
  'target_defaults': {
    'conditions': [
      ['OS!="linux" and OS!="freebsd"', {'sources/': [['exclude', '/linux/']]}],
      ['OS!="mac"', {'sources/': [['exclude', '/mac/']]}],
      ['OS!="win"', {'sources/': [['exclude', '/win/']]}],
    ],
    'variables': {
      # Since we are not often debugging FFmpeg, and performance is
      # unacceptable without optimization, freeze the optimizations to -O2.
      # If someone really wants -O1 , they can change these in their checkout.
      # If you want -O0, see the Gotchas in README.Chromium for why that
      # won't work.
      'debug_optimize': '2',
      'mac_debug_optimization': '2',
    },
  },
  'variables': {
    # Allow overridding the selection of which FFmpeg binaries to copy via an
    # environment variable.  Affects the ffmpeg_binaries target.
    'conditions': [
      ['chromeos!=0 or toolkit_views!=0', {
        'ffmpeg_branding%': '<(branding)OS',
      },{  # else chromeos==0, assume Chrome/Chromium.
        'ffmpeg_branding%': '<(branding)',
      }],
    ],
    'ffmpeg_variant%': '<(target_arch)',

    'use_system_ffmpeg%': 0,
    'use_system_yasm%': 0,

    # Locations for generated artifacts.
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/ffmpeg',
    'asm_library': 'ffmpegasm',
  },
  'conditions': [
    # This condition is for migrating from pre-built binaries to an in-tree
    # source build.  Most of these should be removed once FFmpeg is built on
    # mac and linux.  Windows will take more work.
    #
    # TODO(ajwong): Per the comment above, reduce this conditional's size and
    # determine if in-tree build in Windows is tractable.
    ['(OS!="linux" and OS!="freebsd" and OS!="mac") or use_system_ffmpeg!=0', {
      'variables': {
        'target_for_binaries': 'ffmpeg_binaries',
        'ffmpeg_include_root': 'include',
      },
    },{  # else OS=="linux"
      'variables': {
        'target_for_binaries': 'ffmpegsumo_nolink',
        'ffmpeg_include_root': 'source/patched-ffmpeg-mt',
        'conditions': [
          ['target_arch=="x64" or target_arch=="ia32"', {
            'ffmpeg_asm_lib': 1,
          }],
          ['target_arch=="arm"', {
            'ffmpeg_asm_lib': 0,
          }],
        ],
      },
      'targets': [
        {
          'target_name': 'ffmpegsumo',
          'type': 'shared_library',
          'sources': [
            'source/patched-ffmpeg-mt/libavcodec/allcodecs.c',
            'source/patched-ffmpeg-mt/libavcodec/audioconvert.c',
            'source/patched-ffmpeg-mt/libavcodec/avpacket.c',
            'source/patched-ffmpeg-mt/libavcodec/bitstream.c',
            'source/patched-ffmpeg-mt/libavcodec/bitstream_filter.c',
            'source/patched-ffmpeg-mt/libavcodec/dsputil.c',
            'source/patched-ffmpeg-mt/libavcodec/eval.c',
            'source/patched-ffmpeg-mt/libavcodec/faanidct.c',
            'source/patched-ffmpeg-mt/libavcodec/fft.c',
            'source/patched-ffmpeg-mt/libavcodec/imgconvert.c',
            'source/patched-ffmpeg-mt/libavcodec/jrevdct.c',
            'source/patched-ffmpeg-mt/libavcodec/mdct.c',
            'source/patched-ffmpeg-mt/libavcodec/opt.c',
            'source/patched-ffmpeg-mt/libavcodec/options.c',
            'source/patched-ffmpeg-mt/libavcodec/parser.c',
            'source/patched-ffmpeg-mt/libavcodec/pthread.c',
            'source/patched-ffmpeg-mt/libavcodec/raw.c',
            'source/patched-ffmpeg-mt/libavcodec/simple_idct.c',
            'source/patched-ffmpeg-mt/libavcodec/utils.c',
            'source/patched-ffmpeg-mt/libavcodec/vorbis.c',
            'source/patched-ffmpeg-mt/libavcodec/vorbis_data.c',
            'source/patched-ffmpeg-mt/libavcodec/vorbis_dec.c',
            'source/patched-ffmpeg-mt/libavcodec/vp3.c',
            'source/patched-ffmpeg-mt/libavcodec/vp3dsp.c',
            'source/patched-ffmpeg-mt/libavcodec/xiph.c',
            'source/patched-ffmpeg-mt/libavformat/allformats.c',
            'source/patched-ffmpeg-mt/libavformat/avio.c',
            'source/patched-ffmpeg-mt/libavformat/aviobuf.c',
            'source/patched-ffmpeg-mt/libavformat/cutils.c',
            'source/patched-ffmpeg-mt/libavformat/metadata.c',
            'source/patched-ffmpeg-mt/libavformat/metadata_compat.c',
            'source/patched-ffmpeg-mt/libavformat/oggdec.c',
            'source/patched-ffmpeg-mt/libavformat/oggparseogm.c',
            'source/patched-ffmpeg-mt/libavformat/oggparsetheora.c',
            'source/patched-ffmpeg-mt/libavformat/oggparsevorbis.c',
            'source/patched-ffmpeg-mt/libavformat/options.c',
            'source/patched-ffmpeg-mt/libavformat/riff.c',
            'source/patched-ffmpeg-mt/libavformat/utils.c',
            'source/patched-ffmpeg-mt/libavutil/avstring.c',
            'source/patched-ffmpeg-mt/libavutil/crc.c',
            'source/patched-ffmpeg-mt/libavutil/log.c',
            'source/patched-ffmpeg-mt/libavutil/mathematics.c',
            'source/patched-ffmpeg-mt/libavutil/mem.c',
            'source/patched-ffmpeg-mt/libavutil/rational.c',
            # Config file for the OS and architecture.
            'source/config/<(ffmpeg_branding)/<(OS)/<(target_arch)/config.h',
          ],
          'include_dirs': [
            'source/config/<(ffmpeg_branding)/<(OS)/<(target_arch)',
            'source/patched-ffmpeg-mt',
          ],
          'defines': [
            'HAVE_AV_CONFIG_H',
            '_POSIX_C_SOURCE=200112',
          ],
          'cflags': [
            '-fomit-frame-pointer',
          ],
          'conditions': [
            ['ffmpeg_branding=="Chrome" or ffmpeg_branding=="ChromeOS"', {
              'sources': [
                'source/patched-ffmpeg-mt/libavcodec/aac.c',
                'source/patched-ffmpeg-mt/libavcodec/aac_ac3_parser.c',
                'source/patched-ffmpeg-mt/libavcodec/aac_parser.c',
                'source/patched-ffmpeg-mt/libavcodec/aactab.c',
                'source/patched-ffmpeg-mt/libavcodec/cabac.c',
                'source/patched-ffmpeg-mt/libavcodec/error_resilience.c',
                'source/patched-ffmpeg-mt/libavcodec/golomb.c',
                'source/patched-ffmpeg-mt/libavcodec/h264.c',
                'source/patched-ffmpeg-mt/libavcodec/h264_mp4toannexb_bsf.c',
                'source/patched-ffmpeg-mt/libavcodec/h264_parser.c',
                'source/patched-ffmpeg-mt/libavcodec/h264idct.c',
                'source/patched-ffmpeg-mt/libavcodec/h264pred.c',
                'source/patched-ffmpeg-mt/libavcodec/mpeg4audio.c',
                'source/patched-ffmpeg-mt/libavcodec/mpegaudio.c',
                'source/patched-ffmpeg-mt/libavcodec/mpegaudio_parser.c',
                'source/patched-ffmpeg-mt/libavcodec/mpegaudiodata.c',
                'source/patched-ffmpeg-mt/libavcodec/mpegaudiodec.c',
                'source/patched-ffmpeg-mt/libavcodec/mpegaudiodecheader.c',
                'source/patched-ffmpeg-mt/libavcodec/mpegvideo.c',
                'source/patched-ffmpeg-mt/libavformat/id3v2.c',
                'source/patched-ffmpeg-mt/libavformat/isom.c',
                'source/patched-ffmpeg-mt/libavformat/mov.c',
                'source/patched-ffmpeg-mt/libavformat/mp3.c',
                'source/patched-ffmpeg-mt/libavutil/intfloat_readwrite.c',
              ],
            }],  # ffmpeg_branding
            ['ffmpeg_branding=="ChromeOS"', {
              'sources': [
                'source/patched-ffmpeg-mt/libavcodec/h263.c',
                'source/patched-ffmpeg-mt/libavcodec/h263dec.c',
                'source/patched-ffmpeg-mt/libavcodec/intrax8.c',
                'source/patched-ffmpeg-mt/libavcodec/intrax8dsp.c',
                'source/patched-ffmpeg-mt/libavcodec/mpeg12data.c',
                'source/patched-ffmpeg-mt/libavcodec/mpeg4video_parser.c',
                'source/patched-ffmpeg-mt/libavcodec/msmpeg4.c',
                'source/patched-ffmpeg-mt/libavcodec/msmpeg4data.c',
                'source/patched-ffmpeg-mt/libavcodec/pcm.c',
                'source/patched-ffmpeg-mt/libavcodec/vc1.c',
                'source/patched-ffmpeg-mt/libavcodec/vc1data.c',
                'source/patched-ffmpeg-mt/libavcodec/vc1dsp.c',
                'source/patched-ffmpeg-mt/libavcodec/wma.c',
                'source/patched-ffmpeg-mt/libavcodec/wmadec.c',
                'source/patched-ffmpeg-mt/libavcodec/wmv2.c',
                'source/patched-ffmpeg-mt/libavcodec/wmv2dec.c',
                'source/patched-ffmpeg-mt/libavformat/asf.c',
                'source/patched-ffmpeg-mt/libavformat/asfcrypt.c',
                'source/patched-ffmpeg-mt/libavformat/asfdec.c',
                'source/patched-ffmpeg-mt/libavformat/avidec.c',
                'source/patched-ffmpeg-mt/libavformat/raw.c',
                'source/patched-ffmpeg-mt/libavformat/wav.c',
                'source/patched-ffmpeg-mt/libavutil/des.c',
                'source/patched-ffmpeg-mt/libavutil/rc4.c',
              ],
            }],  # ffmpeg_branding
            ['target_arch=="ia32" or target_arch=="x64"', {
              'dependencies': [
                'make_ffmpeg_asm_lib',
              ],
              'sources': [
                'source/patched-ffmpeg-mt/libavcodec/x86/cpuid.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/dsputil_mmx.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/fdct_mmx.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/fft_3dn.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/fft_3dn2.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/fft_sse.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/idct_mmx_xvid.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/idct_sse2_xvid.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/simple_idct_mmx.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/vp3dsp_mmx.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/vp3dsp_sse2.c',
              ],
            }],
            ['(target_arch=="ia32" or target_arch=="x64") and (ffmpeg_branding=="ChromeOS" or ffmpeg_branding=="Chrome")', {
              'dependencies': [
                'make_ffmpeg_asm_lib',
              ],
              'sources': [
                'source/patched-ffmpeg-mt/libavcodec/x86/mpegvideo_mmx.c',
              ],
            }],
            ['(target_arch=="ia32" or target_arch=="x64") and ffmpeg_branding=="ChromeOS"', {
              'dependencies': [
                'make_ffmpeg_asm_lib',
              ],
              'sources': [
                'source/patched-ffmpeg-mt/libavcodec/x86/dsputil_mmx.c',
                'source/patched-ffmpeg-mt/libavcodec/x86/vc1dsp_mmx.c',
              ],
            }],
            ['target_arch=="x64"', {
              # x64 requires PIC for shared libraries. This is opposite
              # of ia32 where due to a slew of inline assembly using ebx,
              # FFmpeg CANNOT be built with PIC.
              'defines': [
                'PIC',
              ],
              'cflags': [
                '-fPIC',
              ],
            }],  # target_arch=="x64"
            ['target_arch=="arm"', {
              'defines': [
                'PIC',
              ],
              'cflags': [
                '-fPIC',
                '-march=armv7-a',
                '-mtune=cortex-a8',
                '-mfpu=neon',
                '-mfloat-abi=softfp',
              ],
              'sources': [
                'source/patched-ffmpeg-mt/libavcodec/arm/dsputil_arm.c',
                'source/patched-ffmpeg-mt/libavcodec/arm/dsputil_arm_s.S',
                'source/patched-ffmpeg-mt/libavcodec/arm/dsputil_vfp.S',
                'source/patched-ffmpeg-mt/libavcodec/arm/float_arm_vfp.c',
                'source/patched-ffmpeg-mt/libavcodec/arm/jrevdct_arm.S',
                'source/patched-ffmpeg-mt/libavcodec/arm/simple_idct_arm.S',
                'source/patched-ffmpeg-mt/libavcodec/arm/simple_idct_armv5te.S',
                'source/patched-ffmpeg-mt/libavcodec/arm/simple_idct_armv6.S',
                'source/patched-ffmpeg-mt/libavcodec/arm/simple_idct_neon.S',
              ],
            }],  # target_arch=="arm" 
            ['target_arch=="arm" and (ffmpeg_branding=="Chrome" or ffmpeg_branding=="ChromeOS")', {
              'sources': [
	        # TODO(fbarchard): dsputil_neon code should be used by chromium
		# for ogg, but with h264 references only if CONFIG_H264_DECODER
		# is enabled.
                'source/patched-ffmpeg-mt/libavcodec/arm/dsputil_neon.c',
                'source/patched-ffmpeg-mt/libavcodec/arm/dsputil_neon_s.S',
                'source/patched-ffmpeg-mt/libavcodec/arm/h264dsp_neon.S',
                'source/patched-ffmpeg-mt/libavcodec/arm/h264idct_neon.S',
                'source/patched-ffmpeg-mt/libavcodec/arm/mpegvideo_arm.c',
                'source/patched-ffmpeg-mt/libavcodec/arm/mpegvideo_armv5te.c',
                'source/patched-ffmpeg-mt/libavcodec/arm/mpegvideo_armv5te_s.S',
              ],
            }],
           ['target_arch=="arm" and ffmpeg_branding=="ChromeOS"', {
              'sources': [
                'source/patched-ffmpeg-mt/libavcodec/h264_mp4toannexb_bsf.c',
                'source/patched-ffmpeg-mt/libavcodec/mpeg4video_es_bsf.c',
              ],
            }],  # target_arch=="arm" and ffmpeg_branding=="ChromeOS"
            ['OS=="linux" or OS=="freebsd"', {
              'defines': [
                '_ISOC99_SOURCE',
                '_LARGEFILE_SOURCE',
              ],
              'cflags': [
                '-std=c99',
                '-pthread',
                '-fno-math-errno',
              ],
              'cflags!': [
                # Ensure the symbols are exported.
                #
                # TODO(ajwong): Fix common.gypi to only add this flag for
                # _type != shared_library.
                '-fvisibility=hidden',
              ],
              'link_settings': {
                'ldflags': [
                  '-Wl,-Bsymbolic',
                  '-L<(shared_generated_dir)',
                ],
                'libraries': [
                  '-lz',
                ],
                'conditions': [
                  ['ffmpeg_asm_lib==1', {
                    'libraries': [
                      # TODO(ajwong): When scons is dead, collapse this with the
                      # absolute path entry inside the OS="mac" conditional, and
                      # move it out of the conditionals block altogether.
                      '-l<(asm_library)',
                    ],
                  }],
                ],
              },
            }],  # OS=="linux" or OS=="freebsd"
            ['OS=="mac"', {
              'libraries': [
                # TODO(ajwong): Move into link_settings when this is fixed:
                #
                # http://code.google.com/p/gyp/issues/detail?id=108
                '<(shared_generated_dir)/<(STATIC_LIB_PREFIX)<(asm_library)<(STATIC_LIB_SUFFIX)',
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/usr/lib/libz.dylib',
                ],
              },
              'xcode_settings': {
                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',  # No -fvisibility=hidden
                'GCC_DYNAMIC_NO_PIC': 'YES',         # -mdynamic-no-pic
                                                     # (equiv -fno-PIC)
                'DYLIB_INSTALL_NAME_BASE': '@loader_path',
                'LIBRARY_SEARCH_PATHS': [
                  '<(shared_generated_dir)'
                ],
                'OTHER_LDFLAGS': [
                  # This is needed because FFmpeg cannot be built as PIC, and
                  # thus we need to instruct the linker to allow relocations
                  # for read-only segments for this target to be able to
                  # generated the shared library on Mac.
                  #
                  # This makes Mark sad, but he's okay with it since it is
                  # isolated to this module. When Mark finds this in the
                  # future, and has forgotten this conversation, this comment
                  # should remind him that the world is still nice and
                  # butterflies still exist...as do rainbows, sunshine,
                  # tulips, etc., etc...but not kittens. Those went away
                  # with this flag.
                  '-Wl,-read_only_relocs,suppress'
                ],
              },
            }],  # OS=="mac"
          ],
          'actions': [
            {
              # Needed to serialize the output of make_ffmpeg_asm_lib with
              # this target being built.
              'action_name': 'ffmpegasm_barrier',
              'inputs': [
                '<(shared_generated_dir)/<(STATIC_LIB_PREFIX)<(asm_library)<(STATIC_LIB_SUFFIX)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/third_party/ffmpeg/<(asm_library)'
              ],
              'action': [
                'touch',
                '<(INTERMEDIATE_DIR)/third_party/ffmpeg/<(asm_library)'
              ],
              'process_outputs_as_sources': 0,
              'message': 'Serializing build of <(asm_library).',
            },
          ],
        },
        {
          'target_name': 'assemble_ffmpeg_asm',
          'type': 'none',
          'conditions': [
            ['use_system_yasm==0', {
              'dependencies': [
                '../yasm/yasm.gyp:yasm#host',
              ],
              'variables': {
                'yasm_path': '<(PRODUCT_DIR)/yasm',
              },
            },{  # use_system_yasm!=0
              'variables': {
                'yasm_path': '<!(which yasm)',
              },
            }],
          ],
          'sources': [
            # The FFmpeg yasm files.
            'source/patched-ffmpeg-mt/libavcodec/x86/dsputil_yasm.asm',
            'source/patched-ffmpeg-mt/libavcodec/x86/fft_mmx.asm',
          ],
          'rules': [
            {
              'conditions': [
                ['OS=="linux" or OS=="freebsd"', {
                  'variables': {
                    'obj_format': 'elf',
                  },
                  'conditions': [
                    ['target_arch=="ia32"', {
                      'variables': {
                        'yasm_flags': [
                          '-DARCH_X86_32',
                          '-m', 'x86',
                        ],
                      },
                    }],
                    ['target_arch=="x64"', {
                      'variables': {
                        'yasm_flags': [
                          '-DARCH_X86_64',
                          '-m', 'amd64',
                          '-DPIC',
                        ],
                      },
                    }],
                    ['target_arch=="arm"', {
                      'variables': {
                        'yasm_flags': [],
                      },
                    }],
                  ],
                }], ['OS=="mac"', {
                  'variables': {
                    'obj_format': 'macho',
                    'yasm_flags': [ '-DPREFIX', ],
                  },
                  'conditions': [
                    ['target_arch=="ia32"', {
                      'variables': {
                        'yasm_flags': [
                          '-DARCH_X86_32',
                          '-m', 'x86',
                        ],
                      },
                    }],
                    ['target_arch=="x64"', {
                      'variables': {
                        'yasm_flags': [
                          '-DARCH_X86_64',
                          '-m', 'amd64',
                          '-DPIC',
                        ],
                      },
                    }],
                  ],
                }],
              ],
              'rule_name': 'assemble',
              'extension': 'asm',
              'inputs': [ '<(yasm_path)', ],
              'outputs': [
                '<(shared_generated_dir)/<(RULE_INPUT_ROOT).o',
              ],
              'action': [
                '<(yasm_path)',
                '-f', '<(obj_format)',
                '<@(yasm_flags)',
                '-I', 'source/patched-ffmpeg-mt/libavcodec/x86/',
                '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).o',
                '<(RULE_INPUT_PATH)',
              ],
              'process_outputs_as_sources': 0,
              'message': 'Build ffmpeg yasm build <(RULE_INPUT_PATH).',
            },
          ],
        },
        {
          'target_name': 'make_ffmpeg_asm_lib',
          'type': 'none',
          'dependencies': [
            'assemble_ffmpeg_asm',
          ],
          'sources': [
          ],
          'actions': [
            {
              'action_name': 'make_library',
              'variables': {
                # Make sure this stays in sync with the corresponding sources
                # in assemble_ffmpeg_asm.
                'asm_objects': [
                  '<(shared_generated_dir)/dsputil_yasm.o',
                  '<(shared_generated_dir)/fft_mmx.o',
                ],
                'library_path': '<(shared_generated_dir)/<(STATIC_LIB_PREFIX)<(asm_library)<(STATIC_LIB_SUFFIX)',
              },
              'inputs': [ '<@(asm_objects)', ],
              'outputs': [ '<(library_path)', ],
              'action': [ 'ar', 'rcs', '<(library_path)', '<@(asm_objects)', ],
              'process_outputs_as_sources': 0,
              'message': 'Packate ffmpeg assembly into <(library_path).',
            },
          ],
        },
        {
          # A target shim that allows putting a dependency on ffmpegsumo
          # without pulling it into the link line.
          #
          # We use an "executable" taget without any sources to break the
          # link line relationship to ffmpegsumo.
          #
          # Most people will want to depend on this target instead of on
          # ffmpegsumo directly since ffmpegsumo is meant to be
          # used via dlopen() in chrome.
          'target_name': 'ffmpegsumo_nolink',
          'type': 'executable',
          'sources': [ 'dummy_nolink.cc' ],
          'dependencies': [
            'ffmpegsumo',
          ],
          'conditions': [
            ['OS=="linux" or OS=="freebsd"', {
              'copies': [
                {
                  # On Make and Scons builds, the library does not end up in
                  # the PRODUCT_DIR.
                  #
                  # http://code.google.com/p/gyp/issues/detail?id=57
                  #
                  # TODO(ajwong): Fix gyp, fix the world.
                  'destination': '<(PRODUCT_DIR)',
                  'files': ['<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)ffmpegsumo<(SHARED_LIB_SUFFIX)'],
                },
              ],
            }],
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'variables': {
        'generate_stubs_script': 'generate_stubs.py',
        'sig_files': [
          # Note that these must be listed in dependency order.
          # (i.e. if A depends on B, then B must be listed before A.)
          'avutil-50.sigs',
          'avcodec-52.sigs',
          'avformat-52.sigs',
        ],
        'extra_header': 'ffmpeg_stub_headers.fragment',
      },
      'target_name': 'ffmpeg',
      'msvs_guid': 'D7A94F58-576A-45D9-A45F-EB87C63ABBB0',
      'dependencies': [
        '<(target_for_binaries)',
        'ffmpeg_binaries',
      ],
      'sources': [
        '<(ffmpeg_include_root)/libavcodec/avcodec.h',
        '<(ffmpeg_include_root)/libavcodec/opt.h',
        '<(ffmpeg_include_root)/libavcodec/vdpau.h',
        '<(ffmpeg_include_root)/libavcodec/xvmc.h',
        '<(ffmpeg_include_root)/libavformat/avformat.h',
        '<(ffmpeg_include_root)/libavformat/avio.h',
        '<(ffmpeg_include_root)/libavutil/avstring.h',
        '<(ffmpeg_include_root)/libavutil/crc.h',
        '<(ffmpeg_include_root)/libavutil/intfloat_readwrite.h',
        '<(ffmpeg_include_root)/libavutil/log.h',
        '<(ffmpeg_include_root)/libavutil/mathematics.h',
        '<(ffmpeg_include_root)/libavutil/mem.h',
        '<(ffmpeg_include_root)/libavutil/pixfmt.h',
        '<(ffmpeg_include_root)/libavutil/rational.h',

        # Hacks to introduce C99 types into Visual Studio.
        'include/win/inttypes.h',
        'include/win/stdint.h',

        # Files needed for stub generation rules.
        '<@(sig_files)',
        '<(extra_header)'
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'conditions': [
        ['OS=="win"',
          {
            'variables': {
              'outfile_type': 'windows_lib',
              'output_dir': '<(PRODUCT_DIR)/lib',
              'intermediate_dir': '<(INTERMEDIATE_DIR)',
            },
            'type': 'none',
            'sources!': [
              '<(extra_header)',
            ],
            'direct_dependent_settings': {
              'include_dirs': [
                'include/win',
              ],
              'link_settings': {
                'libraries': [
                  '<(output_dir)/avcodec-52.lib',
                  '<(output_dir)/avformat-52.lib',
                  '<(output_dir)/avutil-50.lib',
                ],
                'msvs_settings': {
                  'VCLinkerTool': {
                    'DelayLoadDLLs': [
                      'avcodec-52.dll',
                      'avformat-52.dll',
                      'avutil-50.dll',
                    ],
                  },
                },
              },
            },
            'rules': [
              {
                'rule_name': 'generate_libs',
                'extension': 'sigs',
                'inputs': [
                  '<(generate_stubs_script)',
                  '<@(sig_files)',
                ],
                'outputs': [
                  '<(output_dir)/<(RULE_INPUT_ROOT).lib',
                ],
                'action': ['python', '<(generate_stubs_script)',
                           '-i', '<(intermediate_dir)',
                           '-o', '<(output_dir)',
                           '-t', '<(outfile_type)',
                           '<@(RULE_INPUT_PATH)',
                ],
                'message': 'Generating FFmpeg import libraries.',
              },
            ],
          }, {  # else OS!="win"
            'variables': {
              'outfile_type': 'posix_stubs',
              'stubs_filename_root': 'ffmpeg_stubs',
              'project_path': 'third_party/ffmpeg',
              'intermediate_dir': '<(INTERMEDIATE_DIR)',
              'output_root': '<(SHARED_INTERMEDIATE_DIR)/ffmpeg',
            },
            'type': '<(library)',
            'include_dirs': [
              'include',
              '<(output_root)',
              '../..',  # The chromium 'src' directory.
            ],
            'direct_dependent_settings': {
              'defines': [
                '__STDC_CONSTANT_MACROS',  # FFmpeg uses INT64_C.
              ],
              'include_dirs': [
                '<(output_root)',
                '../..',  # The chromium 'src' directory.
              ],
            },
            'actions': [
              {
                'action_name': 'generate_stubs',
                'inputs': [
                  '<(generate_stubs_script)',
                  '<(extra_header)',
                  '<@(sig_files)',
                ],
                'outputs': [
                  '<(intermediate_dir)/<(stubs_filename_root).cc',
                  '<(output_root)/<(project_path)/<(stubs_filename_root).h',
                ],
                'action': ['python',
                           '<(generate_stubs_script)',
                           '-i', '<(intermediate_dir)',
                           '-o', '<(output_root)/<(project_path)',
                           '-t', '<(outfile_type)',
                           '-e', '<(extra_header)',
                           '-s', '<(stubs_filename_root)',
                           '-p', '<(project_path)',
                           '<@(_inputs)',
                ],
                'process_outputs_as_sources': 1,
                'message': 'Generating FFmpeg stubs for dynamic loading.',
              },
            ],
          },
        ],
        ['OS=="linux" or OS=="freebsd"', {
          'link_settings': {
            'libraries': [
              # We need dl for dlopen() and friends.
              '-ldl',
            ],
          },
        }],
      ],  # conditions
    },
    {
      'target_name': 'ffmpeg_binaries',
      'type': 'none',
      'msvs_guid': '4E4070E1-EFD9-4EF1-8634-3960956F6F10',
      'variables': {
        'conditions': [
          [ 'ffmpeg_branding=="Chrome"', {
            'ffmpeg_bin_dir': 'chrome/<(OS)/<(ffmpeg_variant)',
          }, {  # else ffmpeg_branding!="Chrome", assume chromium.
            'ffmpeg_bin_dir': 'chromium/<(OS)/<(ffmpeg_variant)',
          }],
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'source_files': [
              'binaries/<(ffmpeg_bin_dir)/avcodec-52.dll',
              'binaries/<(ffmpeg_bin_dir)/avformat-52.dll',
              'binaries/<(ffmpeg_bin_dir)/avutil-50.dll',
            ],
          },
          'dependencies': ['../../build/win/system.gyp:cygwin'],
        }], ['OS=="linux" or OS=="freebsd"', {
              'variables': {
                # TODO(ajwong): Clean this up after we've finished
                # migrating to in-tree build.
                'source_files': [
                ],
              },
        }], ['OS=="mac"', {
              # TODO(ajwong): These files are also copied in:
              # webkit/tools/test_shell/test_shell.gyp and
              # chrome/chrome.gyp
              # Need to consolidate the copies in one place. (BUG=23602)
              'variables': {
                'source_files': [
                ],
              },
        }],
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/',
          'files': [
            '<@(source_files)',
          ]
        },
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
