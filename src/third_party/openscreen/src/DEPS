# This file is used to manage the dependencies of the Open Screen repo. It is
# used by gclient to determine what version of each dependency to check out.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.

use_relative_paths = True

vars = {
    'boringssl_git': 'https://boringssl.googlesource.com',
    'chromium_git': 'https://chromium.googlesource.com',

    # TODO(jophba): move to googlesource external for github repos.
    'github': 'https://github.com',

    # NOTE: Strangely enough, this will be overridden by any _parent_ DEPS, so
    # in Chromium it will correctly be True.
    'build_with_chromium': False,

    'gn_version': 'git_revision:0790d3043387c762a6bacb1ae0a9ebe883188ab2',
    'checkout_chromium_quic_boringssl': False,

    # By default, do not check out openscreen/cast. This can be overridden
    # by custom_vars in .gclient.
    'checkout_openscreen_cast_internal': False
}

deps = {
    'cast/internal': {
        'url': 'https://chrome-internal.googlesource.com/openscreen/cast.git' +
            '@' + '703984f9d1674c2cfc259904a5a7fba4990cca4b',
        'condition': 'checkout_openscreen_cast_internal',
    },

    'buildtools': {
        'url': Var('chromium_git')+ '/chromium/src/buildtools' +
            '@' + '140e4d7c45ffb55ce5dc4d11a0c3938363cd8257',
        'condition': 'not build_with_chromium',
    },

    'third_party/protobuf/src': {
        'url': Var('chromium_git') +
            '/external/github.com/protocolbuffers/protobuf.git' +
            '@' + 'd09d649aea36f02c03f8396ba39a8d4db8a607e4', # version 3.10.1
        'condition': 'not build_with_chromium',
    },

    'third_party/zlib/src': {
        'url': Var('github') +
            '/madler/zlib.git' +
            '@' + 'cacf7f1d4e3d44d871b605da3b647f07d718623f', # version 1.2.11
        'condition': 'not build_with_chromium',
    },

    'third_party/jsoncpp/src': {
        'url': Var('chromium_git') +
            '/external/github.com/open-source-parsers/jsoncpp.git' +
            '@' + '2eb20a938c454411c1d416caeeb2a6511daab5cb', # version 1.9.0
        'condition': 'not build_with_chromium',
    },

    'third_party/googletest/src': {
        'url': Var('chromium_git') +
            '/external/github.com/google/googletest.git' +
            '@' + '8697709e0308af4cd5b09dc108480804e5447cf0',
        'condition': 'not build_with_chromium',
    },

    'third_party/mDNSResponder/src': {
        'url': Var('github') + '/jevinskie/mDNSResponder.git' +
            '@' + '2942dde61f920fbbf96ff9a3840567ebbe7cb1b6',
        'condition': 'not build_with_chromium',
    },

    'third_party/boringssl/src': {
        'url' : Var('boringssl_git') + '/boringssl.git' +
            '@' + '6410e18e9190b6b0c71955119fbf3cae1b9eedb7',
        'condition': 'not build_with_chromium',
    },

    'third_party/chromium_quic/src': {
        'url': Var('chromium_git') + '/openscreen/quic.git' +
            '@' + 'b73bd98ac9eaedf01a732b1933f97112cf247d93',
        'condition': 'not build_with_chromium',
    },

    'third_party/tinycbor/src':
        Var('chromium_git') + '/external/github.com/intel/tinycbor.git' +
        '@' + '755f9ef932f9830a63a712fd2ac971d838b131f1',

    'third_party/abseil/src': {
        'url': Var('chromium_git') +
            '/external/github.com/abseil/abseil-cpp.git' +
            '@' + '20de2db748ca0471cfb61cb53e813dd12938c12b',
        'condition': 'not build_with_chromium',
    },
}

recursedeps = [
    'third_party/chromium_quic/src',
    'buildtools',
]

include_rules = [
    '+build/config/features.h',
    '+util',
    '+platform/api',
    '+platform/base',
    '+platform/test',
    '+third_party',

    # Don't include abseil from the root so the path can change via include_dirs
    # rules when in Chromium.
    '-third_party/abseil',

    # Abseil whitelist.
    '+absl/algorithm/container.h',
    '+absl/base/thread_annotations.h',
    '+absl/hash/hash.h',
    '+absl/strings/ascii.h',
    '+absl/strings/match.h',
    '+absl/strings/numbers.h',
    '+absl/strings/str_cat.h',
    '+absl/strings/str_join.h',
    '+absl/strings/str_split.h',
    '+absl/strings/string_view.h',
    '+absl/strings/substitute.h',
    '+absl/types/optional.h',
    '+absl/types/span.h',
    '+absl/types/variant.h',

    # Similar to abseil, don't include boringssl using root path.  Instead,
    # explicitly allow 'openssl' where needed.
    '-third_party/boringssl',

    # Test framework includes.
    "-third_party/googletest",
    "+gtest",
    "+gmock",
]

skip_child_includes = [
    'third_party/chromium_quic',
]
