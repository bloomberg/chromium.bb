#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for owners.py."""

import unittest

import owners
from tests import filesystem_mock

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

  def assert_covered_by(self, files, reviewers):
    db = self.db()
    self.assertTrue(db.files_are_covered_by(set(files), set(reviewers)))

  def test_covered_by__everyone(self):
    self.assert_covered_by(['DEPS'], [john])
    self.assert_covered_by(['DEPS'], [darin])

  def test_covered_by__explicit(self):
    self.assert_covered_by(['content/content.gyp'], [john])
    self.assert_covered_by(['chrome/gpu/OWNERS'], [ken])

  def test_covered_by__owners_propagates_down(self):
    self.assert_covered_by(['chrome/gpu/OWNERS'], [ben])
    self.assert_covered_by(['/chrome/renderer/gpu/gpu_channel_host.h'], [peter])

  def assert_not_covered_by(self, files, reviewers, unreviewed_files):
    db = self.db()
    self.assertEquals(db.files_not_covered_by(set(files), set(reviewers)),
                      set(unreviewed_files))

  def test_not_covered_by__need_at_least_one_reviewer(self):
    self.assert_not_covered_by(['DEPS'], [], ['DEPS'])

  def test_not_covered_by__owners_propagates_down(self):
    self.assert_not_covered_by(
      ['chrome/gpu/gpu_channel.h', 'chrome/renderer/gpu/gpu_channel_host.h'],
      [ben], [])

  def test_not_covered_by__partial_covering(self):
    self.assert_not_covered_by(
      ['content/content.gyp', 'chrome/renderer/gpu/gpu_channel_host.h'],
      [peter], ['content/content.gyp'])

  def test_not_covered_by__set_noparent_works(self):
    self.assert_not_covered_by(['content/content.gyp'], [ben],
                             ['content/content.gyp'])

  def assert_reviewers_for(self, files, expected_reviewers):
    db = self.db()
    self.assertEquals(db.reviewers_for(set(files)), set(expected_reviewers))

  def test_reviewers_for__basic_functionality(self):
    self.assert_reviewers_for(['chrome/gpu/gpu_channel.h'],
                             [ken, ben, brett, owners.EVERYONE])

  def test_reviewers_for__set_noparent_works(self):
    self.assert_reviewers_for(['content/content.gyp'], [john, darin])

  def test_reviewers_for__wildcard_dir(self):
    self.assert_reviewers_for(['DEPS'], [owners.EVERYONE])

  def assert_syntax_error(self, owners_file_contents):
    db = self.db()
    self.files['/foo/OWNERS'] = owners_file_contents
    self.files['/foo/DEPS'] = ''
    self.assertRaises(owners.SyntaxErrorInOwnersFile, db.reviewers_for,
      ['/foo/DEPS'])

  def test_syntax_error__unknown_token(self):
    self.assert_syntax_error('{}\n')

  def test_syntax_error__unknown_set(self):
    self.assert_syntax_error('set myfatherisbillgates\n')

  def test_syntax_error__bad_email(self):
    self.assert_syntax_error('ben\n')


if __name__ == '__main__':
  unittest.main()
