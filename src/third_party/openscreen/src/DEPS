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

  # NOTE: we should only reference GitHub directly for dependencies toggled
  # with the "not build_with_chromium" condition.
  'github': 'https://github.com',

  # NOTE: Strangely enough, this will be overridden by any _parent_ DEPS, so
  # in Chromium it will correctly be True.
  'build_with_chromium': False,

  'checkout_chromium_quic_boringssl': False,

  # Needed to download additional clang binaries for processing coverage data
  # (from binaries with GN arg `use_coverage=true`).
  #
  # TODO(issuetracker.google.com/155195126): Change this to False and update
  # buildbot to call tools/download-clang-update-script.py instead.
  'checkout_clang_coverage_tools': True,
}

deps = {
  # NOTE: This commit hash here references a repository/branch that is a mirror
  # of the commits to the buildtools directory in the Chromium repository. This
  # should be regularly updated with the tip of the MIRRORED master branch,
  # found here:
  # https://chromium.googlesource.com/chromium/src/buildtools/+/refs/heads/master.
  'buildtools': {
    'url': Var('chromium_git')+ '/chromium/src/buildtools' +
      '@' + '204a35a2a64f7179f8b76d7a0385653690839e21',
    'condition': 'not build_with_chromium',
  },

  'third_party/protobuf/src': {
    'url': Var('chromium_git') +
      '/external/github.com/protocolbuffers/protobuf.git' +
      '@' + 'd0bfd5221182da1a7cc280f3337b5e41a89539cf', # version 3.11.4
    'condition': 'not build_with_chromium',
  },

  'third_party/libprotobuf-mutator/src': {
    'url': Var('chromium_git') +
      '/external/github.com/google/libprotobuf-mutator.git' + '@' '439e81f8f4847ec6e2bf11b3aa634a5d8485633d',
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
      '@' + '9059f5cad030ba11d37818847443a53918c327b1', # version 1.9.4
    'condition': 'not build_with_chromium',
  },

  'third_party/googletest/src': {
    'url': Var('chromium_git') +
      '/external/github.com/google/googletest.git' +
      '@' + '0eea2e9fc63461761dea5f2f517bd6af2ca024fa',
    'condition': 'not build_with_chromium',
  },

  'third_party/mDNSResponder/src': {
    'url': Var('github') + '/jevinskie/mDNSResponder.git' +
      '@' + '2942dde61f920fbbf96ff9a3840567ebbe7cb1b6',
    'condition': 'not build_with_chromium',
  },

  # Note about updating BoringSSL: after changing this hash, run the update
  # script in BoringSSL's util folder for generating build files from the
  # <openscreen src-dir>/third_party/boringssl directory:
  # python ./src/util/generate_build_files.py gn
  'third_party/boringssl/src': {
    'url' : Var('boringssl_git') + '/boringssl.git' +
      '@' + '78987bb7bb4764ca3a8b08b0a6f7bd14b53c3e4f',
    'condition': 'not build_with_chromium',
  },

  'third_party/chromium_quic/src': {
    'url': Var('chromium_git') + '/openscreen/quic.git' +
      '@' + '1bd3640a8eeb9d7c36e4ecb48f6ebab5d9694002',
    'condition': 'not build_with_chromium',
  },

  'third_party/tinycbor/src':
    Var('chromium_git') + '/external/github.com/intel/tinycbor.git' +
    '@' + '755f9ef932f9830a63a712fd2ac971d838b131f1',

  # Abseil recommends living at head.  Chromium takes an Abseil snapshot
  # irregularly, every 1-2 months.  When rolling, roll manually to the current
  # Chromium snapshot since it's pretty close to head.
  'third_party/abseil/src': {
    'url': Var('chromium_git') +
      '/external/github.com/abseil/abseil-cpp.git' +
      '@' + 'cde2e2410e58c884b3bf5f67c6511e6266036249',
    'condition': 'not build_with_chromium',
  },

  'third_party/libfuzzer/src': {
    'url': Var('chromium_git') +
      '/chromium/llvm-project/compiler-rt/lib/fuzzer.git' +
      '@' + 'debe7d2d1982e540fbd6bd78604bf001753f9e74',
    'condition': 'not build_with_chromium',
  },
}

hooks = [
  {
    'name': 'clang_update_script',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python', 'tools/download-clang-update-script.py',
                '--output', 'tools/clang/scripts/update.py' ],
    # NOTE: This file appears in .gitignore, as it is not a part of the
    # openscreen repo.
  },
 {
    'name': 'yajsv_update_script',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python', 'tools/download-yajsv.py' ],
  },
  {
    'name': 'update_clang',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python', 'tools/clang/scripts/update.py' ],
  },
  {
    'name': 'clang_coverage_tools',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_clang_coverage_tools',
    'action': ['python', 'tools/clang/scripts/update.py',
               '--package=coverage_tools'],
  },
  {
    'name': 'clang_format_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage.py', '--no_resume', '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/linux64/clang-format.sha1' ],
    'condition': 'host_os == "linux" and not build_with_chromium',
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage.py', '--no_resume', '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/mac/clang-format.sha1' ],
    'condition': 'host_os == "mac" and not build_with_chromium',
  },
]

recursedeps = [
  'third_party/chromium_quic/src',
  'buildtools',
]

include_rules = [
  '+util',
  '+platform/api',
  '+platform/base',
  '+platform/test',
  '+testing/util',
  '+third_party',

  # Don't include abseil from the root so the path can change via include_dirs
  # rules when in Chromium.
  '-third_party/abseil',

  # Abseil allowed headers.
  '+absl/algorithm/container.h',
  '+absl/base/thread_annotations.h',
  '+absl/hash/hash.h',
  '+absl/hash/hash_testing.h',
  '+absl/strings/ascii.h',
  '+absl/strings/match.h',
  '+absl/strings/numbers.h',
  '+absl/strings/str_cat.h',
  '+absl/strings/str_join.h',
  '+absl/strings/str_replace.h',
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
