# Copyright (c) 2011 The LevelDB Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file. See the AUTHORS file for names of contributors.

{
  'variables': {
    'use_snappy%': 0,
  },
  'target_defaults': {
    'defines': [
      'LEVELDB_PLATFORM_CHROMIUM=1',
    ],
    'include_dirs': [
      '.',
      'src/',
      'src/include/',
    ],
    'conditions': [
      ['OS == "win"', {
        'include_dirs': [
          'src/port/win',
        ],
      }],
      ['use_snappy', {
        'defines': [
          'USE_SNAPPY=1',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'leveldatabase',
      'type': '<(library)',
      'dependencies': [
        '../../base/base.gyp:base',
        # base::LazyInstance is a template that pulls in dynamic_annotations so
        # we need to explictly link in the code for dynamic_annotations.
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'conditions': [
        ['use_snappy', {
          'dependencies': [
            '../../third_party/snappy/snappy.gyp:snappy',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include/',
          'src/',
        ],
        'conditions': [
          ['OS == "win"', {
            'include_dirs': [
              'src/port/win',
            ],
          }],
        ],
      },
      'sources': [
        # Include and then exclude so that all files show up in IDEs, even if
        # they don't build.
        'env_chromium.cc',
        'port/port_chromium.cc',
        'port/port_chromium.h',
        'src/db/builder.cc',
        'src/db/builder.h',
        'src/db/db_impl.cc',
        'src/db/db_impl.h',
        'src/db/db_iter.cc',
        'src/db/db_iter.h',
        'src/db/filename.cc',
        'src/db/filename.h',
        'src/db/dbformat.cc',
        'src/db/dbformat.h',
        'src/db/log_format.h',
        'src/db/log_reader.cc',
        'src/db/log_reader.h',
        'src/db/log_writer.cc',
        'src/db/log_writer.h',
        'src/db/memtable.cc',
        'src/db/memtable.h',
        'src/db/repair.cc',
        'src/db/skiplist.h',
        'src/db/snapshot.h',
        'src/db/table_cache.cc',
        'src/db/table_cache.h',
        'src/db/version_edit.cc',
        'src/db/version_edit.h',
        'src/db/version_set.cc',
        'src/db/version_set.h',
        'src/db/write_batch.cc',
        'src/db/write_batch_internal.h',
        'src/helpers/memenv/memenv.cc',
        'src/helpers/memenv/memenv.h',
        'src/include/leveldb/cache.h',
        'src/include/leveldb/comparator.h',
        'src/include/leveldb/db.h',
        'src/include/leveldb/env.h',
        'src/include/leveldb/iterator.h',
        'src/include/leveldb/options.h',
        'src/include/leveldb/slice.h',
        'src/include/leveldb/status.h',
        'src/include/leveldb/table.h',
        'src/include/leveldb/table_builder.h',
        'src/include/leveldb/write_batch.h',
        'src/port/port.h',
        'src/port/port_example.h',
        'src/port/port_posix.cc',
        'src/port/port_posix.h',
        'src/table/block.cc',
        'src/table/block.h',
        'src/table/block_builder.cc',
        'src/table/block_builder.h',
        'src/table/format.cc',
        'src/table/format.h',
        'src/table/iterator.cc',
        'src/table/iterator_wrapper.h',
        'src/table/merger.cc',
        'src/table/merger.h',
        'src/table/table.cc',
        'src/table/table_builder.cc',
        'src/table/two_level_iterator.cc',
        'src/table/two_level_iterator.h',
        'src/util/arena.cc',
        'src/util/arena.h',
        'src/util/cache.cc',
        'src/util/coding.cc',
        'src/util/coding.h',
        'src/util/comparator.cc',
        'src/util/crc32c.cc',
        'src/util/crc32c.h',
        'src/util/env.cc',
        'src/util/hash.cc',
        'src/util/hash.h',
        'src/util/logging.cc',
        'src/util/logging.h',
        'src/util/mutexlock.h',
        'src/util/options.cc',
        'src/util/random.h',
        'src/util/status.cc',
      ],
      'sources/': [
        ['exclude', '_(android|example|portable|posix)\\.cc$'],
      ],
    },
    # TODO(dgrogan): Replace the test targets once third_party/leveldb is gone.
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
