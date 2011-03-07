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
  if kwargs.get('noparent'):
    s = 'set noparent\n'
  return s + '\n'.join(email_addresses) + '\n'


def test_repo():
  return filesystem_mock.MockFileSystem(files={
    '/DEPS' : '',
    '/OWNERS': owners_file('*'),
    '/base/vlog.h': '',
    '/chrome/OWNERS': owners_file(ben, brett),
    '/chrome/gpu/OWNERS': owners_file(ken),
    '/chrome/gpu/gpu_channel.h': '',
    '/chrome/renderer/OWNERS': owners_file(peter),
    '/chrome/renderer/gpu/gpu_channel_host.h': '',
    '/chrome/renderer/safe_browsing/scorer.h': '',
    '/content/OWNERS': owners_file(john, darin, noparent=True),
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

  def assertReviewersFor(self, files, expected_reviewers):
    db = self.db()
    self.assertEquals(db.ReviewersFor(set(files)), set(expected_reviewers))

  def assertCoveredBy(self, files, reviewers):
    db = self.db()
    self.assertTrue(db.FilesAreCoveredBy(set(files), set(reviewers)))

  def assertNotCoveredBy(self, files, reviewers, unreviewed_files):
    db = self.db()
    self.assertEquals(db.FilesNotCoveredBy(set(files), set(reviewers)),
                      set(unreviewed_files))

  def test_constructor(self):
    self.assertNotEquals(self.db(), None)

  def test_owners_for(self):
    self.assertReviewersFor(['DEPS'], [owners.ANYONE])
    self.assertReviewersFor(['content/content.gyp'], [john, darin])
    self.assertReviewersFor(['chrome/gpu/gpu_channel.h'], [ken])

  def test_covered_by(self):
    self.assertCoveredBy(['DEPS'], [john])
    self.assertCoveredBy(['DEPS'], [darin])
    self.assertCoveredBy(['content/content.gyp'], [john])
    self.assertCoveredBy(['chrome/gpu/OWNERS'], [ken])
    self.assertCoveredBy(['chrome/gpu/OWNERS'], [ben])

  def test_not_covered_by(self):
    self.assertNotCoveredBy(['DEPS'], [], ['DEPS'])
    self.assertNotCoveredBy(['content/content.gyp'], [ben],
      ['content/content.gyp'])
    self.assertNotCoveredBy(
      ['chrome/gpu/gpu_channel.h', 'chrome/renderer/gpu/gpu_channel_host.h'],
      [peter], ['chrome/gpu/gpu_channel.h'])
    self.assertNotCoveredBy(
      ['chrome/gpu/gpu_channel.h', 'chrome/renderer/gpu/gpu_channel_host.h'],
      [ben], [])

  def test_comments_in_owners_file(self):
    # pylint: disable=W0212
    db = self.db()
    # Tests that this doesn't raise an error.
    db._ReadOwnersFile('OWNERS', 'DEPS')

  def test_syntax_error_in_owners_file(self):
    # pylint: disable=W0212
    db = self.db()
    self.files['/foo/OWNERS'] = '{}\n'
    self.files['/foo/DEPS'] = '# DEPS\n'
    self.assertRaises(owners.SyntaxErrorInOwnersFile, db._ReadOwnersFile,
      '/foo/OWNERS', '/foo/DEPS')

    self.files['/bar/OWNERS'] = 'set myparentislinus\n'
    self.files['/bar/DEPS'] = '# DEPS\n'
    self.assertRaises(owners.SyntaxErrorInOwnersFile, db._ReadOwnersFile,
      '/bar/OWNERS', '/bar/DEPS')

  def test_owners_propagates_down(self):
    self.assertCoveredBy(['/chrome/renderer/gpu/gpu_channel_host.h'], [peter])

  def test_set_noparent(self):
    self.assertNotCoveredBy(['/content/content.gyp'], [peter],
      ['/content/content.gyp'])


if __name__ == '__main__':
  unittest.main()
