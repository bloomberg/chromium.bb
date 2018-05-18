#!/usr/bin/env python
# Copyright 2018 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import hashlib
import logging
import os
import sys
import tempfile
import unittest

TEST_DIR = os.path.dirname(os.path.abspath(
    __file__.decode(sys.getfilesystemencoding())))
ROOT_DIR = os.path.dirname(TEST_DIR)
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
from depot_tools import fix_encoding

from utils import file_path

import local_caching


class DiskContentAddressedCacheTest(auto_stub.TestCase):
  def setUp(self):
    super(DiskContentAddressedCacheTest, self).setUp()
    # If this fails on Windows, please rerun this tests as an elevated user with
    # administrator access right.
    self.assertEqual(True, file_path.enable_symlink())

    self._algo = hashlib.sha256
    self._free_disk = 1000
    # Max: 100 bytes, 2 items
    # Min free disk: 1000 bytes.
    self._policies = local_caching.CachePolicies(
        max_cache_size=100,
        min_free_space=1000,
        max_items=2,
        max_age_secs=0)
    self.tempdir = tempfile.mkdtemp(prefix=u'isolateserver')

    def get_free_space(p):
      self.assertEqual(p, os.path.join(self.tempdir, 'cache'))
      return self._free_disk
    self.mock(file_path, 'get_free_space', get_free_space)

  def tearDown(self):
    try:
      file_path.rmtree(self.tempdir)
    finally:
      super(DiskContentAddressedCacheTest, self).tearDown()

  def get_cache(self, **kwargs):
    root_dir = os.path.join(self.tempdir, 'cache')
    return local_caching.DiskContentAddressedCache(
        root_dir, self._policies, self._algo, trim=True, **kwargs)

  def to_hash(self, content):
    return self._algo(content).hexdigest(), content

  def test_file_write(self):
    # TODO(maruel): Write test for file_write generator (or remove it).
    pass

  def test_read_evict(self):
    self._free_disk = 1100
    h_a = self.to_hash('a')[0]
    with self.get_cache() as cache:
      cache.write(h_a, 'a')
      with cache.getfileobj(h_a) as f:
        self.assertEqual('a', f.read())

    with self.get_cache() as cache:
      cache.evict(h_a)
      with self.assertRaises(local_caching.CacheMiss):
        cache.getfileobj(h_a)

  def test_policies_free_disk(self):
    with self.assertRaises(local_caching.NoMoreSpace):
      self.get_cache().write(*self.to_hash('a'))

  def test_policies_fit(self):
    self._free_disk = 1100
    self.get_cache().write(*self.to_hash('a'*100))

  def test_policies_too_much(self):
    # Cache (size and # items) is not enforced while adding items but free disk
    # is.
    self._free_disk = 1004
    cache = self.get_cache()
    for i in ('a', 'b', 'c', 'd'):
      cache.write(*self.to_hash(i))
    # Mapping more content than the amount of free disk required.
    with self.assertRaises(local_caching.NoMoreSpace) as cm:
      cache.write(*self.to_hash('e'))
    expected = (
        'Not enough space to fetch the whole isolated tree.\n'
        '  CachePolicies(max_cache_size=100; max_items=2; min_free_space=1000; '
          'max_age_secs=0)\n'
        '  cache=6bytes, 6 items; 999b free_space')
    self.assertEqual(expected, cm.exception.message)

  def test_cleanup(self):
    # Inject an item without a state.json, one is lost. Both will be deleted on
    # cleanup.
    self._free_disk = 1003
    cache = self.get_cache()
    h_foo = hashlib.sha1('foo').hexdigest()
    self.assertEqual([], sorted(cache._lru._items.iteritems()))
    with cache:
      cache.write(h_foo, ['foo'])
    self.assertEqual([h_foo], [i[0] for i in cache._lru._items.iteritems()])

    h_a = self.to_hash('a')[0]
    local_caching.file_write(os.path.join(cache.cache_dir, h_a), 'a')
    os.remove(os.path.join(cache.cache_dir, h_foo))

    # Still hasn't realized that the file is missing.
    self.assertEqual([h_foo], [i[0] for i in cache._lru._items.iteritems()])
    self.assertEqual(
        sorted([h_a, u'state.json']), sorted(os.listdir(cache.cache_dir)))
    cache.cleanup()
    self.assertEqual([u'state.json'], os.listdir(cache.cache_dir))

  def test_policies_active_trimming(self):
    # Start with a larger cache, add many object.
    # Reload the cache with smaller policies, the cache should be trimmed on
    # load.
    h_a = self.to_hash('a')[0]
    h_b = self.to_hash('b')[0]
    h_c = self.to_hash('c')[0]
    h_large, large = self.to_hash('b' * 99)

    def assertItems(expected):
      actual = [
        (digest, size) for digest, (size, _) in cache._lru._items.iteritems()]
      self.assertEqual(expected, actual)

    # Max policies is 100 bytes, 2 items, 1000 bytes free space.
    self._free_disk = 1101
    with self.get_cache() as cache:
      cache.write(h_a, 'a')
      cache.write(h_large, large)
      # Cache (size and # items) is not enforced while adding items. The
      # rationale is that a task may request more data than the size of the
      # cache policies. As long as there is free space, this is fine.
      cache.write(h_b, 'b')
      assertItems([(h_a, 1), (h_large, len(large)), (h_b, 1)])
      self.assertEqual(h_a, cache._protected)
      self.assertEqual(1000, cache._free_disk)
      self.assertEqual(0, cache.initial_number_items)
      self.assertEqual(0, cache.initial_size)
      # Free disk is enforced, because otherwise we assume the task wouldn't
      # be able to start. In this case, it throws an exception since all items
      # are protected. The item is added since it's detected after the fact.
      with self.assertRaises(local_caching.NoMoreSpace):
        cache.write(h_c, 'c')

    # At this point, after the implicit trim in __exit__(), h_a and h_large were
    # evicted.
    self.assertEqual(
        sorted([h_b, h_c, u'state.json']), sorted(os.listdir(cache.cache_dir)))

    # Allow 3 items and 101 bytes so h_large is kept.
    self._policies = local_caching.CachePolicies(
        max_cache_size=101,
        min_free_space=1000,
        max_items=3,
        max_age_secs=0)
    with self.get_cache() as cache:
      cache.write(h_large, large)
      self.assertEqual(2, cache.initial_number_items)
      self.assertEqual(2, cache.initial_size)

    self.assertEqual(
        sorted([h_b, h_c, h_large, u'state.json']),
        sorted(os.listdir(cache.cache_dir)))

    # Assert that trimming is done in constructor too.
    self._policies = local_caching.CachePolicies(
        max_cache_size=100,
        min_free_space=1000,
        max_items=2,
        max_age_secs=0)
    with self.get_cache() as cache:
      assertItems([(h_c, 1), (h_large, len(large))])
      self.assertEqual(None, cache._protected)
      self.assertEqual(1101, cache._free_disk)
      self.assertEqual(2, cache.initial_number_items)
      self.assertEqual(100, cache.initial_size)

  def test_policies_trim_old(self):
    # Add two items, one 3 weeks and one minute old, one recent, make sure the
    # old one is trimmed.
    self._policies = local_caching.CachePolicies(
        max_cache_size=1000,
        min_free_space=0,
        max_items=1000,
        max_age_secs=21*24*60*60)
    now = 100
    c = self.get_cache(time_fn=lambda: now)
    # Test the very limit of 3 weeks:
    c.write(hashlib.sha1('old').hexdigest(), 'old')
    now += 1
    c.write(hashlib.sha1('recent').hexdigest(), 'recent')
    now += 21*24*60*60
    c.trim()
    self.assertEqual(set([hashlib.sha1('recent').hexdigest()]), c.cached_set())

  def test_some_file_brutally_deleted(self):
    h_a = self.to_hash('a')[0]

    self._free_disk = 1100
    with self.get_cache() as cache:
      cache.write(h_a, 'a')
      self.assertTrue(cache.touch(h_a, local_caching.UNKNOWN_FILE_SIZE))
      self.assertTrue(cache.touch(h_a, 1))

    os.remove(os.path.join(cache.cache_dir, h_a))

    with self.get_cache() as cache:
      # 'Ghost' entry loaded with state.json is still there.
      self.assertEqual({h_a}, cache.cached_set())
      # 'touch' detects the file is missing by returning False.
      self.assertFalse(cache.touch(h_a, local_caching.UNKNOWN_FILE_SIZE))
      self.assertFalse(cache.touch(h_a, 1))
      # Evicting it still works, kills the 'ghost' entry.
      cache.evict(h_a)
      self.assertEqual(set(), cache.cached_set())


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.CRITICAL))
  unittest.main()
