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
    'github': "https://github.com",

    # NOTE: Strangely enough, this will be overridden by any _parent_ DEPS, so
    # in Chromium it will correctly be True.
    'build_with_chromium': False,
}

deps = {
    'third_party/googletest/src': {
        'url': Var('chromium_git') +
            '/external/github.com/google/googletest.git' +
            '@' + 'dfa853b63d17c787914b663b50c2095a0c5b706e',
        'condition': 'not build_with_chromium',
    },

    'third_party/mDNSResponder/src': {
        'url': Var('github') + '/jevinskie/mDNSResponder.git' +
            '@' + '2942dde61f920fbbf96ff9a3840567ebbe7cb1b6',
        'condition': 'not build_with_chromium',
    },

    'third_party/chromium_quic/src': {
        'url': Var('chromium_git') + '/openscreen/quic.git' +
            '@' + 'de9d4dc2e36076888bedcd041126ce5a6c1ca0d4',
        'condition': 'not build_with_chromium',
    },

    'third_party/tinycbor/src':
        Var('chromium_git') + '/external/github.com/intel/tinycbor.git' +
        '@' + 'bfc40dcf909f1998d7760c2bc0e1409979d3c8cb',

    'third_party/abseil/src': {
        'url': Var('chromium_git') +
            '/external/github.com/abseil/abseil-cpp.git' +
            '@' + '5eea0f713c14ac17788b83e496f11903f8e2bbb0',
        'condition': 'not build_with_chromium',
    },
}

recursedeps = [
    'third_party/chromium_quic/src',
]
