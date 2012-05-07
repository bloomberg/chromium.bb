#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for owners.py."""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from testing_support import filesystem_mock

import owners

ben = 'ben@example.com'
brett = 'brett@example.com'
darin = 'darin@example.com'
john = 'john@example.com'
ken = 'ken@example.com'
peter = 'peter@example.com'

def owners_file(*email_addresses, **kwargs):
  s = ''
  if kwargs.get('comment'):
    s += '# %s\n' % kwargs.get('comment')
  if kwargs.get('noparent'):
    s += 'set noparent\n'
  return s + '\n'.join(email_addresses) + '\n'


def test_repo():
  return filesystem_mock.MockFileSystem(files={
    '/DEPS' : '',
    '/OWNERS': owners_file(owners.EVERYONE),
    '/base/vlog.h': '',
    '/chrome/OWNERS': owners_file(ben, brett),
    '/chrome/gpu/OWNERS': owners_file(ken),
    '/chrome/gpu/gpu_channel.h': '',
    '/chrome/renderer/OWNERS': owners_file(peter),
    '/chrome/renderer/gpu/gpu_channel_host.h': '',
    '/chrome/renderer/safe_browsing/scorer.h': '',
    '/content/OWNERS': owners_file(john, darin, comment='foo', noparent=True),
    '/content/content.gyp': '',
    '/content/bar/foo.cc': '',
    '/content/baz/OWNERS': owners_file(brett),
    '/content/baz/froboz.h': '',
    '/content/views/OWNERS': owners_file(ben, john, owners.EVERYONE,
                                         noparent=True),
  })


class OwnersDatabaseTest(unittest.TestCase):
  def setUp(self):
    self.repo = test_repo()
    self.files = self.repo.files
    self.root = '/'
    self.fopen = self.repo.open_for_reading

  def db(self, root=None, fopen=None, os_path=None):
    root = root or self.root
    fopen = fopen or self.fopen
    os_path = os_path or self.repo
    return owners.Database(root, fopen, os_path)

  def test_constructor(self):
    self.assertNotEquals(self.db(), None)

  def test_dirs_not_covered_by__valid_inputs(self):
    db = self.db()

    # Check that we're passed in a sequence that isn't a string.
    self.assertRaises(AssertionError, db.directories_not_covered_by, 'foo', [])
    if hasattr(owners.collections, 'Iterable'):
      self.assertRaises(AssertionError, db.directories_not_covered_by,
                        (f for f in ['x', 'y']), [])

    # Check that the files are under the root.
    db.root = '/checkout'
    self.assertRaises(AssertionError, db.directories_not_covered_by,
                      ['/OWNERS'], [])
    db.root = '/'

    # Check invalid email address.
    self.assertRaises(AssertionError, db.directories_not_covered_by,
                      ['OWNERS'], ['foo'])

  def assert_dirs_not_covered_by(self, files, reviewers, unreviewed_dirs):
    db = self.db()
    self.assertEquals(
        db.directories_not_covered_by(set(files), set(reviewers)),
        set(unreviewed_dirs))

  def test_dirs_not_covered_by__owners_propagates_down(self):
    self.assert_dirs_not_covered_by(
      ['chrome/gpu/gpu_channel.h', 'chrome/renderer/gpu/gpu_channel_host.h'],
      [ben], [])

  def test_dirs_not_covered_by__partial_covering(self):
    self.assert_dirs_not_covered_by(
      ['content/content.gyp', 'chrome/renderer/gpu/gpu_channel_host.h'],
      [peter], ['content'])

  def test_dirs_not_covered_by__set_noparent_works(self):
    self.assert_dirs_not_covered_by(['content/content.gyp'], [ben],
                                    ['content'])

  def test_dirs_not_covered_by__no_reviewer(self):
    self.assert_dirs_not_covered_by(
      ['content/content.gyp', 'chrome/renderer/gpu/gpu_channel_host.h'],
      [], ['content'])

  def test_dirs_not_covered_by__combines_directories(self):
    self.assert_dirs_not_covered_by(['content/content.gyp',
                                     'content/bar/foo.cc',
                                     'chrome/renderer/gpu/gpu_channel_host.h'],
                                    [peter],
                                    ['content'])

  def test_dirs_not_covered_by__multiple_directories(self):
    self.assert_dirs_not_covered_by(
        ['content/content.gyp',                    # Not covered
         'content/bar/foo.cc',                     # Not covered (combines in)
         'content/baz/froboz.h',                   # Not covered
         'chrome/gpu/gpu_channel.h',               # Owned by ken
         'chrome/renderer/gpu/gpu_channel_host.h'  # Owned by * via parent
        ],
        [ken],
        ['content', 'content/baz'])

  def assert_reviewers_for(self, files, expected_reviewers):
    db = self.db()
    self.assertEquals(db.reviewers_for(set(files)), set(expected_reviewers))

  def test_reviewers_for__basic_functionality(self):
    self.assert_reviewers_for(['chrome/gpu/gpu_channel.h'],
                             [brett])

  def test_reviewers_for__set_noparent_works(self):
    self.assert_reviewers_for(['content/content.gyp'], [darin])

  def test_reviewers_for__valid_inputs(self):
    db = self.db()

    # Check that we're passed in a sequence that isn't a string.
    self.assertRaises(AssertionError, db.reviewers_for, 'foo')
    if hasattr(owners.collections, 'Iterable'):
      self.assertRaises(AssertionError, db.reviewers_for,
                        (f for f in ['x', 'y']))

    # Check that the files are under the root.
    db.root = '/checkout'
    self.assertRaises(AssertionError, db.reviewers_for, ['/OWNERS'])

  def test_reviewers_for__wildcard_dir(self):
    self.assert_reviewers_for(['DEPS'], [owners.EVERYONE])

  def assert_syntax_error(self, owners_file_contents):
    db = self.db()
    self.files['/foo/OWNERS'] = owners_file_contents
    self.files['/foo/DEPS'] = ''
    try:
      db.reviewers_for(['/foo/DEPS'])
      self.fail()  # pragma: no cover
    except owners.SyntaxErrorInOwnersFile, e:
      self.assertTrue(str(e).startswith('/foo/OWNERS:1'))

  def test_syntax_error__unknown_token(self):
    self.assert_syntax_error('{}\n')

  def test_syntax_error__unknown_set(self):
    self.assert_syntax_error('set myfatherisbillgates\n')

  def test_syntax_error__bad_email(self):
    self.assert_syntax_error('ben\n')


if __name__ == '__main__':
  unittest.main()
