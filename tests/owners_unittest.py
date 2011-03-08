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

  def test_Constructor(self):
    self.assertNotEquals(self.db(), None)

  def assert_CoveredBy(self, files, reviewers):
    db = self.db()
    self.assertTrue(db.FilesAreCoveredBy(set(files), set(reviewers)))

  def test_CoveredBy_Everyone(self):
    self.assert_CoveredBy(['DEPS'], [john])
    self.assert_CoveredBy(['DEPS'], [darin])

  def test_CoveredBy_Explicit(self):
    self.assert_CoveredBy(['content/content.gyp'], [john])
    self.assert_CoveredBy(['chrome/gpu/OWNERS'], [ken])

  def test_CoveredBy_OwnersPropagatesDown(self):
    self.assert_CoveredBy(['chrome/gpu/OWNERS'], [ben])
    self.assert_CoveredBy(['/chrome/renderer/gpu/gpu_channel_host.h'], [peter])

  def assert_NotCoveredBy(self, files, reviewers, unreviewed_files):
    db = self.db()
    self.assertEquals(db.FilesNotCoveredBy(set(files), set(reviewers)),
                      set(unreviewed_files))

  def test_NotCoveredBy_NeedAtLeastOneReviewer(self):
    self.assert_NotCoveredBy(['DEPS'], [], ['DEPS'])

  def test_NotCoveredBy_OwnersPropagatesDown(self):
    self.assert_NotCoveredBy(
      ['chrome/gpu/gpu_channel.h', 'chrome/renderer/gpu/gpu_channel_host.h'],
      [ben], [])

  def test_NotCoveredBy_PartialCovering(self):
    self.assert_NotCoveredBy(
      ['content/content.gyp', 'chrome/renderer/gpu/gpu_channel_host.h'],
      [peter], ['content/content.gyp'])

  def test_NotCoveredBy_SetNoParentWorks(self):
    self.assert_NotCoveredBy(['content/content.gyp'], [ben],
                             ['content/content.gyp'])

  def assert_ReviewersFor(self, files, expected_reviewers):
    db = self.db()
    self.assertEquals(db.ReviewersFor(set(files)), set(expected_reviewers))

  def test_ReviewersFor_BasicFunctionality(self):
    self.assert_ReviewersFor(['chrome/gpu/gpu_channel.h'],
                             [ken, ben, brett, owners.EVERYONE])

  def test_ReviewersFor_SetNoParentWorks(self):
    self.assert_ReviewersFor(['content/content.gyp'], [john, darin])

  def test_ReviewersFor_WildcardDir(self):
    self.assert_ReviewersFor(['DEPS'], [owners.EVERYONE])

  def assert_SyntaxError(self, owners_file_contents):
    db = self.db()
    self.files['/foo/OWNERS'] = owners_file_contents
    self.files['/foo/DEPS'] = ''
    self.assertRaises(owners.SyntaxErrorInOwnersFile, db.ReviewersFor,
      ['/foo/DEPS'])

  def test_SyntaxError_UnknownToken(self):
    self.assert_SyntaxError('{}\n')

  def test_SyntaxError_UnknownSet(self):
    self.assert_SyntaxError('set myfatherisbillgates\n')

  def test_SyntaxError_BadEmail(self):
    self.assert_SyntaxError('ben\n')


if __name__ == '__main__':
  unittest.main()
