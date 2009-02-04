{
  'variables': {
    'depth': '..',
  },
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'gtest',
      'type': 'static_library',
      'sources': [
        'gtest/src/gtest-test-part.cc',
        'gtest/src/gtest-death-test.cc',
        'gtest/src/gtest-filepath.cc',
        'gtest/src/gtest-port.cc',
        'gtest/src/gtest.cc',
        'gtest/src/gtest_main.cc',
        'multiprocess_func_list.cc',
      ],
      'include_dirs': [
        'gtest',
        'gtest/include',
      ],
      'conditions': [
        [ 'OS == "mac"', { 'sources': [ 'platform_test_mac.mm' ] } ],
      ],
      'direct_dependent_settings': {
        'defines': [
          'UNIT_TEST',
        ],
        'include_dirs': [
          'gtest',
          'gtest/include',
        ],
      },
    },
  ],
}
