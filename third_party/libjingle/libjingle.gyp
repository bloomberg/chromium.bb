# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'variables': {
    'enabled_libjingle_device_manager%': 0,
    'libjingle_additional_deps%': [],
    'libjingle_peerconnection_additional_deps%': [],
    'libjingle_source%': "source",
    'webrtc_p2p': "../webrtc/p2p",
    'webrtc_xmpp': "../webrtc/libjingle/xmpp",
  },
  # Most of these settings have been split according to their scope into
  # :jingle_unexported_configs and :jingle_public_config in the GN build.
  'target_defaults': {
    'defines': [
      'ENABLE_EXTERNAL_AUTH',
      'EXPAT_RELATIVE_PATH',
      'FEATURE_ENABLE_SSL',
      'GTEST_RELATIVE_PATH',
      'HAVE_OPENSSL_SSL_H',
      'HAVE_SRTP',
      'HAVE_WEBRTC_VIDEO',
      'HAVE_WEBRTC_VOICE',
      'LOGGING_INSIDE_WEBRTC',
      'NO_MAIN_THREAD_WRAPPING',
      'NO_SOUND_SYSTEM',
      'SRTP_RELATIVE_PATH',
      'SSL_USE_OPENSSL',
      'USE_WEBRTC_DEV_BRANCH',
      'WEBRTC_CHROMIUM_BUILD',
    ],
    'include_dirs': [
      './overrides',
      '../../third_party/webrtc_overrides',
      './<(libjingle_source)',
      '../..',
      '../../testing/gtest/include',
      '../../third_party',
      '../../third_party/libyuv/include',
      '../../third_party/usrsctp/usrsctplib',
    ],
    # These dependencies have been translated into :jingle_deps in the GN build.
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/net/net.gyp:net',
      '<(DEPTH)/third_party/boringssl/boringssl.gyp:boringssl',
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        '../../third_party/webrtc_overrides',
        './overrides',
        './<(libjingle_source)',
        '../..',
        '../../testing/gtest/include',
        '../../third_party',
      ],
      'defines': [
        'FEATURE_ENABLE_SSL',
        'FEATURE_ENABLE_VOICEMAIL',
        'EXPAT_RELATIVE_PATH',
        'GTEST_RELATIVE_PATH',
        'NO_MAIN_THREAD_WRAPPING',
        'NO_SOUND_SYSTEM',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lsecur32.lib',
              '-lcrypt32.lib',
              '-liphlpapi.lib',
            ],
          },
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/platformsdk_win7/files/Include',
          ],
          'defines': [
            '_CRT_SECURE_NO_WARNINGS',  # Suppres warnings about _vsnprinf
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267 ],
        }],
        ['OS=="linux"', {
          'defines': [
            'LINUX',
            'WEBRTC_LINUX',
          ],
        }],
        ['OS=="mac"', {
          'defines': [
            'OSX',
            'WEBRTC_MAC',
          ],
        }],
        ['OS=="ios"', {
          'defines': [
            'IOS',
            'WEBRTC_MAC',
            'WEBRTC_IOS',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'WEBRTC_WIN',
          ],
        }],
        ['OS=="android"', {
          'defines': [
            'ANDROID',
          ],
        }],
        ['os_posix==1', {
          'defines': [
            'WEBRTC_POSIX',
          ],
        }],
        ['os_bsd==1', {
          'defines': [
            'BSD',
          ],
        }],
        ['OS=="openbsd"', {
          'defines': [
            'OPENBSD',
          ],
        }],
        ['OS=="freebsd"', {
          'defines': [
            'FREEBSD',
          ],
        }],
        ['chromeos==1', {
          'defines': [
            'CHROMEOS',
          ],
        }],
      ],
    },
    'variables': {
      'clang_warning_flags_unset': [
        # Don't warn about string->bool used in asserts.
        '-Wstring-conversion',
      ],
    },
    'conditions': [
      ['OS=="win"', {
        'include_dirs': [
          '../third_party/platformsdk_win7/files/Include',
        ],
        'conditions' : [
          ['target_arch == "ia32"', {
            'defines': [
              '_USE_32BIT_TIME_T',
            ],
          }],
        ],
      }],
      ['OS=="linux"', {
        'defines': [
          'LINUX',
          'WEBRTC_LINUX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'OSX',
          'WEBRTC_MAC',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'WEBRTC_WIN',
        ],
      }],
      ['OS=="ios"', {
        'defines': [
          'IOS',
          'WEBRTC_MAC',
          'WEBRTC_IOS',
        ],
      }],
      ['os_posix == 1', {
        'defines': [
          'WEBRTC_POSIX',
        ],
      }],
      ['os_bsd==1', {
        'defines': [
          'BSD',
        ],
      }],
      ['OS=="openbsd"', {
        'defines': [
          'OPENBSD',
        ],
      }],
      ['OS=="freebsd"', {
        'defines': [
          'FREEBSD',
        ],
      }],
    ],
  },
  'targets': [
    # GN version: //third_party/libjingle
    {
      'target_name': 'libjingle',
      'type': 'static_library',
      'includes': [ 'libjingle_common.gypi' ],
      'sources!' : [
        # Compiled as part of libjingle_p2p_constants.
        '<(webrtc_p2p)/base/constants.cc',
        '<(webrtc_p2p)/base/constants.h',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/webrtc/base/base.gyp:rtc_base',
        '<(DEPTH)/third_party/webrtc/libjingle/xmllite/xmllite.gyp:rtc_xmllite',
        'libjingle_p2p_constants',
        '<@(libjingle_additional_deps)',
      ],
    },  # target libjingle
    # This has to be is a separate project due to a bug in MSVS 2008 and the
    # current toolset on android.  The problem is that we have two files named
    # "constants.cc" and MSVS/android doesn't handle this properly.
    # GYP currently has guards to catch this, so if you want to remove it,
    # run GYP and if GYP has removed the validation check, then we can assume
    # that the toolchains have been fixed (we currently use VS2010 and later,
    # so VS2008 isn't a concern anymore).
    #
    # GN version: //third_party/libjingle:libjingle_p2p_constants
    {
      'target_name': 'libjingle_p2p_constants',
      'type': 'static_library',
      'sources': [
        '<(webrtc_p2p)/base/constants.cc',
        '<(webrtc_p2p)/base/constants.h',
      ],
    },  # target libjingle_p2p_constants
  ],
  'conditions': [
    ['enable_webrtc==1', {
      'targets': [
        {
          # GN version: //third_party/libjingle:libjingle_webrtc_common
          'target_name': 'libjingle_webrtc_common',
          'type': 'static_library',
          'sources': [
            '<(libjingle_source)/talk/app/webrtc/audiotrack.cc',
            '<(libjingle_source)/talk/app/webrtc/audiotrack.h',
            '<(libjingle_source)/talk/app/webrtc/datachannel.cc',
            '<(libjingle_source)/talk/app/webrtc/datachannel.h',
            '<(libjingle_source)/talk/app/webrtc/dtlsidentitystore.cc',
            '<(libjingle_source)/talk/app/webrtc/dtlsidentitystore.h',
            '<(libjingle_source)/talk/app/webrtc/dtmfsender.cc',
            '<(libjingle_source)/talk/app/webrtc/dtmfsender.h',
            '<(libjingle_source)/talk/app/webrtc/jsep.h',
            '<(libjingle_source)/talk/app/webrtc/jsepicecandidate.cc',
            '<(libjingle_source)/talk/app/webrtc/jsepicecandidate.h',
            '<(libjingle_source)/talk/app/webrtc/jsepsessiondescription.cc',
            '<(libjingle_source)/talk/app/webrtc/jsepsessiondescription.h',
            '<(libjingle_source)/talk/app/webrtc/localaudiosource.cc',
            '<(libjingle_source)/talk/app/webrtc/localaudiosource.h',
            '<(libjingle_source)/talk/app/webrtc/mediaconstraintsinterface.cc',
            '<(libjingle_source)/talk/app/webrtc/mediaconstraintsinterface.h',
            '<(libjingle_source)/talk/app/webrtc/mediacontroller.cc',
            '<(libjingle_source)/talk/app/webrtc/mediacontroller.h',
            '<(libjingle_source)/talk/app/webrtc/mediastream.cc',
            '<(libjingle_source)/talk/app/webrtc/mediastream.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamhandler.cc',
            '<(libjingle_source)/talk/app/webrtc/mediastreamhandler.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreaminterface.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamobserver.cc',
            '<(libjingle_source)/talk/app/webrtc/mediastreamobserver.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamprovider.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamproxy.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamtrack.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamtrackproxy.h',
            '<(libjingle_source)/talk/app/webrtc/notifier.h',
            '<(libjingle_source)/talk/app/webrtc/peerconnection.cc',
            '<(libjingle_source)/talk/app/webrtc/peerconnection.h',
            '<(libjingle_source)/talk/app/webrtc/peerconnectionfactory.cc',
            '<(libjingle_source)/talk/app/webrtc/peerconnectionfactory.h',
            '<(libjingle_source)/talk/app/webrtc/peerconnectioninterface.h',
            '<(libjingle_source)/talk/app/webrtc/portallocatorfactory.cc',
            '<(libjingle_source)/talk/app/webrtc/portallocatorfactory.h',
            '<(libjingle_source)/talk/app/webrtc/remoteaudiosource.cc',
            '<(libjingle_source)/talk/app/webrtc/remoteaudiosource.h',
            '<(libjingle_source)/talk/app/webrtc/remoteaudiotrack.cc',
            '<(libjingle_source)/talk/app/webrtc/remoteaudiotrack.h',
            '<(libjingle_source)/talk/app/webrtc/remotevideocapturer.cc',
            '<(libjingle_source)/talk/app/webrtc/remotevideocapturer.h',
            '<(libjingle_source)/talk/app/webrtc/rtpreceiver.cc',
            '<(libjingle_source)/talk/app/webrtc/rtpreceiver.h',
            '<(libjingle_source)/talk/app/webrtc/rtpreceiverinterface.h',
            '<(libjingle_source)/talk/app/webrtc/rtpsender.cc',
            '<(libjingle_source)/talk/app/webrtc/rtpsender.h',
            '<(libjingle_source)/talk/app/webrtc/rtpsenderinterface.h',
            '<(libjingle_source)/talk/app/webrtc/sctputils.cc',
            '<(libjingle_source)/talk/app/webrtc/sctputils.h',
            '<(libjingle_source)/talk/app/webrtc/statscollector.cc',
            '<(libjingle_source)/talk/app/webrtc/statscollector.h',
            '<(libjingle_source)/talk/app/webrtc/statstypes.cc',
            '<(libjingle_source)/talk/app/webrtc/statstypes.h',
            '<(libjingle_source)/talk/app/webrtc/streamcollection.h',
            '<(libjingle_source)/talk/app/webrtc/umametrics.h',
            '<(libjingle_source)/talk/app/webrtc/videosource.cc',
            '<(libjingle_source)/talk/app/webrtc/videosource.h',
            '<(libjingle_source)/talk/app/webrtc/videosourceinterface.h',
            '<(libjingle_source)/talk/app/webrtc/videosourceproxy.h',
            '<(libjingle_source)/talk/app/webrtc/videotrack.cc',
            '<(libjingle_source)/talk/app/webrtc/videotrack.h',
            '<(libjingle_source)/talk/app/webrtc/videotrackrenderers.cc',
            '<(libjingle_source)/talk/app/webrtc/videotrackrenderers.h',
            '<(libjingle_source)/talk/app/webrtc/webrtcsdp.cc',
            '<(libjingle_source)/talk/app/webrtc/webrtcsdp.h',
            '<(libjingle_source)/talk/app/webrtc/webrtcsession.cc',
            '<(libjingle_source)/talk/app/webrtc/webrtcsession.h',
            '<(libjingle_source)/talk/app/webrtc/webrtcsessiondescriptionfactory.cc',
            '<(libjingle_source)/talk/app/webrtc/webrtcsessiondescriptionfactory.h',
            '<(libjingle_source)/talk/media/base/audiorenderer.h',
            '<(libjingle_source)/talk/media/base/capturemanager.cc',
            '<(libjingle_source)/talk/media/base/capturemanager.h',
            '<(libjingle_source)/talk/media/base/capturerenderadapter.cc',
            '<(libjingle_source)/talk/media/base/capturerenderadapter.h',
            '<(libjingle_source)/talk/media/base/codec.cc',
            '<(libjingle_source)/talk/media/base/codec.h',
            '<(libjingle_source)/talk/media/base/constants.cc',
            '<(libjingle_source)/talk/media/base/constants.h',
            '<(libjingle_source)/talk/media/base/cryptoparams.h',
            '<(libjingle_source)/talk/media/base/hybriddataengine.h',
            '<(libjingle_source)/talk/media/base/mediachannel.h',
            '<(libjingle_source)/talk/media/base/mediaengine.cc',
            '<(libjingle_source)/talk/media/base/mediaengine.h',
            '<(libjingle_source)/talk/media/base/rtpdataengine.cc',
            '<(libjingle_source)/talk/media/base/rtpdataengine.h',
            '<(libjingle_source)/talk/media/base/rtpdump.cc',
            '<(libjingle_source)/talk/media/base/rtpdump.h',
            '<(libjingle_source)/talk/media/base/rtputils.cc',
            '<(libjingle_source)/talk/media/base/rtputils.h',
            '<(libjingle_source)/talk/media/base/streamparams.cc',
            '<(libjingle_source)/talk/media/base/streamparams.h',
            '<(libjingle_source)/talk/media/base/videoadapter.cc',
            '<(libjingle_source)/talk/media/base/videoadapter.h',
            '<(libjingle_source)/talk/media/base/videocapturer.cc',
            '<(libjingle_source)/talk/media/base/videocapturer.h',
            '<(libjingle_source)/talk/media/base/videocommon.cc',
            '<(libjingle_source)/talk/media/base/videocommon.h',
            '<(libjingle_source)/talk/media/base/videoframe.cc',
            '<(libjingle_source)/talk/media/base/videoframe.h',
            '<(libjingle_source)/talk/media/base/videoframefactory.cc',
            '<(libjingle_source)/talk/media/base/videoframefactory.h',
            '<(libjingle_source)/talk/media/devices/dummydevicemanager.cc',
            '<(libjingle_source)/talk/media/devices/dummydevicemanager.h',
            '<(libjingle_source)/talk/media/devices/filevideocapturer.cc',
            '<(libjingle_source)/talk/media/devices/filevideocapturer.h',
            '<(libjingle_source)/talk/media/webrtc/webrtccommon.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoframe.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoframe.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoframefactory.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoframefactory.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvoe.h',
            '<(libjingle_source)/talk/session/media/audiomonitor.cc',
            '<(libjingle_source)/talk/session/media/audiomonitor.h',
            '<(libjingle_source)/talk/session/media/bundlefilter.cc',
            '<(libjingle_source)/talk/session/media/bundlefilter.h',
            '<(libjingle_source)/talk/session/media/channel.cc',
            '<(libjingle_source)/talk/session/media/channel.h',
            '<(libjingle_source)/talk/session/media/channelmanager.cc',
            '<(libjingle_source)/talk/session/media/channelmanager.h',
            '<(libjingle_source)/talk/session/media/currentspeakermonitor.cc',
            '<(libjingle_source)/talk/session/media/currentspeakermonitor.h',
            '<(libjingle_source)/talk/session/media/externalhmac.cc',
            '<(libjingle_source)/talk/session/media/externalhmac.h',
            '<(libjingle_source)/talk/session/media/mediamonitor.cc',
            '<(libjingle_source)/talk/session/media/mediamonitor.h',
            '<(libjingle_source)/talk/session/media/mediasession.cc',
            '<(libjingle_source)/talk/session/media/mediasession.h',
            '<(libjingle_source)/talk/session/media/mediasink.h',
            '<(libjingle_source)/talk/session/media/rtcpmuxfilter.cc',
            '<(libjingle_source)/talk/session/media/rtcpmuxfilter.h',
            '<(libjingle_source)/talk/session/media/srtpfilter.cc',
            '<(libjingle_source)/talk/session/media/srtpfilter.h',
            '<(libjingle_source)/talk/session/media/voicechannel.h',
          ],
          'conditions': [
            # TODO(mallinath) - Enable SCTP for iOS.
            ['OS!="ios"', {
              'defines': [
                'HAVE_SCTP',
              ],
              'sources': [
                '<(libjingle_source)/talk/media/sctp/sctpdataengine.cc',
                '<(libjingle_source)/talk/media/sctp/sctpdataengine.h',
              ],
              'dependencies': [
                '<(DEPTH)/third_party/usrsctp/usrsctp.gyp:usrsctplib',
              ],
            }],
            ['enabled_libjingle_device_manager==1', {
              'sources!': [
                '<(libjingle_source)/talk/media/devices/dummydevicemanager.cc',
                '<(libjingle_source)/talk/media/devices/dummydevicemanager.h',
              ],
              'sources': [
                '<(libjingle_source)/talk/media/devices/devicemanager.cc',
                '<(libjingle_source)/talk/media/devices/devicemanager.h',
              ],
              'conditions': [
                ['OS=="win"', {
                  'sources': [
                    '<(libjingle_source)/talk/media/devices/win32deviceinfo.cc',
                    '<(libjingle_source)/talk/media/devices/win32devicemanager.cc',
                    '<(libjingle_source)/talk/media/devices/win32devicemanager.h',
                  ],
                }],
                ['OS=="linux"', {
                  'sources': [
                    '<(libjingle_source)/talk/media/devices/libudevsymboltable.cc',
                    '<(libjingle_source)/talk/media/devices/libudevsymboltable.h',
                    '<(libjingle_source)/talk/media/devices/linuxdeviceinfo.cc',
                    '<(libjingle_source)/talk/media/devices/linuxdevicemanager.cc',
                    '<(libjingle_source)/talk/media/devices/linuxdevicemanager.h',
                    '<(libjingle_source)/talk/media/devices/v4llookup.cc',
                    '<(libjingle_source)/talk/media/devices/v4llookup.h',
                  ],
                }],
                ['OS=="mac"', {
                  'sources': [
                    '<(libjingle_source)/talk/media/devices/macdeviceinfo.cc',
                    '<(libjingle_source)/talk/media/devices/macdevicemanager.cc',
                    '<(libjingle_source)/talk/media/devices/macdevicemanager.h',
                    '<(libjingle_source)/talk/media/devices/macdevicemanagermm.mm',
                  ],
                  'xcode_settings': {
                    'WARNING_CFLAGS': [
                      # Suppres warnings about using deprecated functions in
                      # macdevicemanager.cc.
                      '-Wno-deprecated-declarations',
                    ],
                  },
                }],
              ],
            }],
          ],
          'dependencies': [
            '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:media_file',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:video_capture',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:video_render',
            'libjingle',
          ],
        },  # target libjingle_webrtc_common
        {
          # GN version: //third_party/libjingle:libjingle_webrtc
          'target_name': 'libjingle_webrtc',
          'type': 'static_library',
          'sources': [
            'overrides/init_webrtc.cc',
            'overrides/init_webrtc.h',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:audio_processing',
            'libjingle_webrtc_common',
          ],
        },
        {
          # GN version: //third_party/libjingle:libpeerconnection
          'target_name': 'libpeerconnection',
          'type': 'static_library',
          'sources': [
            # Note: sources list duplicated in GN build.
            '<(libjingle_source)/talk/media/webrtc/simulcast.cc',
            '<(libjingle_source)/talk/media/webrtc/simulcast.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcmediaengine.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcmediaengine.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoengine2.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoengine2.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvoiceengine.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvoiceengine.h',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/webrtc/voice_engine/voice_engine.gyp:voice_engine',
            '<(DEPTH)/third_party/webrtc/webrtc.gyp:webrtc',
            '<@(libjingle_peerconnection_additional_deps)',
            'libjingle_webrtc_common',
          ],
          'conditions': [
            ['OS=="android"', {
              'standalone_static_library': 1,
            }],
          ],
        },  # target libpeerconnection
      ],
    }],
  ],
}
