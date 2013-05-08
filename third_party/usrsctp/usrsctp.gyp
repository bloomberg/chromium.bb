# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'usrsctplib',
      'type': 'static_library',
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
        'defines': [
          'INET',
          'SCTP_PROCESS_LEVEL_LOCKS',
          'SCTP_SIMPLE_ALLOCATOR',
          'STDC_HEADERS',
          'USE_SCTP_SHA1',
          '__Userspace__',
          # 'SCTP_DEBUG', # Uncomment for SCTP debugging.
        ],
      },
      'include_dirs': [
        'usrsctplib/',
        'usrsctplib/netinet',
        'usrsctplib/netinet6',
      ],
      'sources': [
        'usrsctplib/user_environment.c',
        'usrsctplib/user_mbuf.c',
        'usrsctplib/user_recv_thread.c',
        'usrsctplib/user_sctp_timer_iterate.c',
        'usrsctplib/user_socket.c',
        'usrsctplib/netinet/sctp_asconf.c',
        'usrsctplib/netinet/sctp_auth.c',
        'usrsctplib/netinet/sctp_bsd_addr.c',
        'usrsctplib/netinet/sctp_callout.c',
        'usrsctplib/netinet/sctp_cc_functions.c',
        'usrsctplib/netinet/sctp_crc32.c',
        'usrsctplib/netinet/sctp_hashdriver.c',
        'usrsctplib/netinet/sctp_indata.c',
        'usrsctplib/netinet/sctp_input.c',
        'usrsctplib/netinet/sctp_output.c',
        'usrsctplib/netinet/sctp_pcb.c',
        'usrsctplib/netinet/sctp_peeloff.c',
        'usrsctplib/netinet/sctp_sha1.c',
        'usrsctplib/netinet/sctp_ss_functions.c',
        'usrsctplib/netinet/sctp_sysctl.c',
        'usrsctplib/netinet/sctp_userspace.c',
        'usrsctplib/netinet/sctp_timer.c',
        'usrsctplib/netinet/sctp_usrreq.c',
        'usrsctplib/netinet/sctputil.c',
        'usrsctplib/netinet6/sctp6_usrreq.c',
      ],  # sources
      'conditions': [
        ['OS=="linux"', {
          'defines': [
            'HAVE_INET_ADDR',
            'HAVE_SOCKET',
            '__Userspace_os_Linux',
          ],
          'ccflags!': [ '-Werror', '-Wall' ],
          'ccflags': [ '-w' ],
        }],
        ['OS=="mac"', {
          'defines': [
            'HAVE_INET_ADDR',
            'HAVE_SA_LEN',
            'HAVE_SCONN_LEN',
            'HAVE_SIN6_LEN',
            'HAVE_SIN_LEN',
            'HAVE_SOCKET',
            'INET6',
            '__APPLE_USE_RFC_2292',
            '__Userspace_os_Darwin',
          ],
          # TODO(ldixon): explore why gyp cflags here does not get picked up.
          'xcode_settings': {
            'OTHER_CFLAGS!': [ '-Werror', '-Wall' ],
            'OTHER_CFLAGS': [ '-U__APPLE__', '-w' ],
          },
        }],
        ['OS=="win"', {
          'defines': [
            'INET6',
            '__Userspace_os_Windows',
          ],
          'ccflags!': [ '/W3', '/WX' ],
          'ccflags': [ '/w' ],
        }, {  # OS != "win",
          'defines': [
            'NON_WINDOWS_DEFINE',
          ],
        }],
      ],  # conditions
    },  # target usrsctp
  ],  # targets
}
