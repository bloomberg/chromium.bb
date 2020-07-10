# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for upload_goma_info.py"""

from __future__ import print_function

import collections
import datetime
import getpass
import json
import os
import time

from chromite.cbuildbot import goma_util
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import osutils


class TestGomaLogUploader(cros_test_lib.MockTempDirTestCase):
  """Tests for upload_goma_info."""

  def _CreateLogFile(self, name, timestamp):
    path = os.path.join(
        self.tempdir,
        '%s.host.log.INFO.%s' % (name, timestamp.strftime('%Y%m%d-%H%M%S.%f')))
    osutils.WriteFile(
        path,
        timestamp.strftime('Log file created at: %Y/%m/%d %H:%M:%S'))

  def testUpload(self):
    self._CreateLogFile(
        'compiler_proxy', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'compiler_proxy-subproc', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 1, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 2, 0))

    # Set env vars.
    os.environ.update({
        'GLOG_log_dir': self.tempdir,
        'BUILDBOT_BUILDERNAME': 'builder-name',
        'BUILDBOT_MASTERNAME': 'master-name',
        'BUILDBOT_SLAVENAME': 'slave-name',
        'BUILDBOT_CLOBBER': '1',
    })

    self.PatchObject(cros_build_lib, 'GetHostName', lambda: 'dummy-host-name')
    copy_log = []
    self.PatchObject(
        gs.GSContext, 'CopyInto',
        lambda _, __, remote_dir, filename=None, **kwargs: copy_log.append(
            (remote_dir, filename, kwargs.get('headers'))))

    goma_util.GomaLogUploader(self.tempdir, today=datetime.date(2017, 4, 26),
                              dry_run=True,
                              cbb_config_name='test_config').Upload()

    expect_builderinfo = json.dumps(collections.OrderedDict([
        ('builder', 'builder-name'),
        ('master', 'master-name'),
        ('slave', 'slave-name'),
        ('clobber', True),
        ('os', 'chromeos'),
        ('is_luci', False),
        ('cbb_config_name', 'test_config'),
    ]))
    self.assertEqual(
        copy_log,
        [('gs://chrome-goma-log/2017/04/26/dummy-host-name',
          'compiler_proxy-subproc.host.log.INFO.20170426-120000.000000.gz',
          ['x-goog-meta-builderinfo:' + expect_builderinfo]),
         ('gs://chrome-goma-log/2017/04/26/dummy-host-name',
          'compiler_proxy.host.log.INFO.20170426-120000.000000.gz',
          ['x-goog-meta-builderinfo:' + expect_builderinfo]),
         ('gs://chrome-goma-log/2017/04/26/dummy-host-name',
          'gomacc.host.log.INFO.20170426-120100.000000.tar.gz',
          ['x-goog-meta-builderinfo:' + expect_builderinfo]),
        ])

  def testUploadLuci(self):
    self._CreateLogFile(
        'compiler_proxy', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'compiler_proxy-subproc', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 1, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 2, 0))

    # Set env vars.
    envs = (
        'BUILDBOT_BUILDERNAME',
        'BUILDBOT_MASTERNAME',
        'BUILDBOT_SLAVENAME',
        'BUILDBOT_CLOBBER',
    )
    for env in envs:
      if env in os.environ:
        del os.environ[env]

    os.environ.update({
        'GLOG_log_dir': self.tempdir,
    })

    self.PatchObject(cros_build_lib, 'GetHostName', lambda: 'dummy-host-name')
    copy_log = []
    self.PatchObject(
        gs.GSContext, 'CopyInto',
        lambda _, __, remote_dir, filename=None, **kwargs: copy_log.append(
            (remote_dir, filename, kwargs.get('headers'))))

    goma_util.GomaLogUploader(self.tempdir, today=datetime.date(2017, 4, 26),
                              dry_run=True,
                              cbb_config_name='test_config').Upload()

    builderinfo = collections.OrderedDict([
        ('builder', ''),
        ('master', ''),
        ('slave', ''),
        ('clobber', False),
        ('os', 'chromeos'),
        ('is_luci', True),
        ('cbb_config_name', 'test_config'),
    ])
    builderinfo['builder_id'] = collections.OrderedDict([
        ('project', 'chromeos'),
        ('builder', 'Prod'),
        ('bucket', 'general'),
    ])

    expect_builderinfo = json.dumps(builderinfo)
    self.assertEqual(
        copy_log,
        [('gs://chrome-goma-log/2017/04/26/dummy-host-name',
          'compiler_proxy-subproc.host.log.INFO.20170426-120000.000000.gz',
          ['x-goog-meta-builderinfo:' + expect_builderinfo]),
         ('gs://chrome-goma-log/2017/04/26/dummy-host-name',
          'compiler_proxy.host.log.INFO.20170426-120000.000000.gz',
          ['x-goog-meta-builderinfo:' + expect_builderinfo]),
         ('gs://chrome-goma-log/2017/04/26/dummy-host-name',
          'gomacc.host.log.INFO.20170426-120100.000000.tar.gz',
          ['x-goog-meta-builderinfo:' + expect_builderinfo]),
        ])

  def testNinjaLogUpload(self):
    self._CreateLogFile(
        'compiler_proxy', datetime.datetime(2017, 8, 21, 12, 0, 0))
    self._CreateLogFile(
        'compiler_proxy-subproc', datetime.datetime(2017, 8, 21, 12, 0, 0))

    ninja_log_path = os.path.join(self.tempdir, 'ninja_log')
    osutils.WriteFile(ninja_log_path, 'Ninja Log Content\n')
    timestamp = datetime.datetime(2017, 8, 21, 12, 0, 0)
    mtime = time.mktime(timestamp.timetuple())
    os.utime(ninja_log_path, ((time.time(), mtime)))

    osutils.WriteFile(
        os.path.join(self.tempdir, 'ninja_command'), 'ninja_command')
    osutils.WriteFile(
        os.path.join(self.tempdir, 'ninja_cwd'), 'ninja_cwd')
    osutils.WriteFile(
        os.path.join(self.tempdir, 'ninja_env'),
        'key1=value1\0key2=value2\0')
    osutils.WriteFile(
        os.path.join(self.tempdir, 'ninja_exit'), '0')

    self.PatchObject(cros_build_lib, 'GetHostName', lambda: 'dummy-host-name')
    copy_log = []
    self.PatchObject(
        gs.GSContext, 'CopyInto',
        lambda _, __, remote_dir, filename=None, **kwargs: copy_log.append(
            (remote_dir, filename)))

    goma_util.GomaLogUploader(self.tempdir, today=datetime.date(2017, 8, 21),
                              dry_run=True).Upload()

    username = getpass.getuser()
    pid = os.getpid()
    upload_filename = 'ninja_log.%s.dummy-host-name.20170821-120000.%d' % (
        username, pid)
    self.assertEqual(
        copy_log,
        [('gs://chrome-goma-log/2017/08/21/dummy-host-name',
          'compiler_proxy-subproc.host.log.INFO.20170821-120000.000000.gz'),
         ('gs://chrome-goma-log/2017/08/21/dummy-host-name',
          'compiler_proxy.host.log.INFO.20170821-120000.000000.gz'),
         ('gs://chrome-goma-log/2017/08/21/dummy-host-name',
          upload_filename + '.gz'),
        ])

    # Verify content of ninja_log file.
    ninja_log_content = osutils.ReadFile(
        os.path.join(self.tempdir, upload_filename))
    content, eof, metadata_json = ninja_log_content.split('\n', 3)
    self.assertEqual('Ninja Log Content', content)
    self.assertEqual('# end of ninja log', eof)
    metadata = json.loads(metadata_json)
    self.assertEqual(
        metadata,
        {
            'platform': 'chromeos',
            'cmdline': ['ninja_command'],
            'cwd': 'ninja_cwd',
            'exit': 0,
            'env': {'key1': 'value1', 'key2': 'value2'},
            'compiler_proxy_info':
            'compiler_proxy.host.log.INFO.20170821-120000.000000'
        })


class GomaTest(cros_test_lib.TempDirTestCase):
  """Tests for the Goma object."""

  def testExtraEnvCustomChroot(self):
    """Test the chroot env building with a custom chroot location."""
    goma_dir = os.path.join(self.tempdir, 'goma')
    goma_client_json = os.path.join(self.tempdir, 'goma_client.json')
    chroot_dir = os.path.join(self.tempdir, 'chroot')
    chroot_tmp = os.path.join(chroot_dir, 'tmp')
    osutils.Touch(goma_client_json)
    osutils.SafeMakedirs(goma_dir)
    osutils.SafeMakedirs(chroot_tmp)

    goma = goma_util.Goma(goma_dir, goma_client_json, chroot_dir=chroot_dir)

    env = goma.GetExtraEnv()
    chroot_env = goma.GetChrootExtraEnv()

    # Make sure the chroot paths got translated.
    self.assertStartsWith(chroot_env['GOMA_TMP_DIR'], '/tmp')
    self.assertStartsWith(chroot_env['GLOG_log_dir'], '/tmp')
    # Make sure the non-chroot paths didn't get translated.
    self.assertStartsWith(env['GOMA_TMP_DIR'], chroot_tmp)
    self.assertStartsWith(env['GLOG_log_dir'], chroot_tmp)
    # Make sure they're based on the same path.
    self.assertEndsWith(env['GOMA_TMP_DIR'], chroot_env['GOMA_TMP_DIR'])
    self.assertEndsWith(env['GLOG_log_dir'], chroot_env['GLOG_log_dir'])

  def testExtraEnvGomaApproach(self):
    """Test the chroot env building with a goma approach."""
    goma_dir = os.path.join(self.tempdir, 'goma')
    goma_client_json = os.path.join(self.tempdir, 'goma_client.json')
    chroot_dir = os.path.join(self.tempdir, 'chroot')
    chroot_tmp = os.path.join(chroot_dir, 'tmp')
    osutils.Touch(goma_client_json)
    osutils.SafeMakedirs(goma_dir)
    osutils.SafeMakedirs(chroot_tmp)
    goma_approach = goma_util.GomaApproach('foo', 'bar', True)

    goma = goma_util.Goma(goma_dir, goma_client_json,
                          chroot_dir=chroot_dir, goma_approach=goma_approach)

    env = goma.GetExtraEnv()

    # Make sure the extra environment specified by goma_approach is present.
    self.assertEqual(env['GOMA_RPC_EXTRA_PARAMS'], 'foo')
    self.assertEqual(env['GOMA_SERVER_HOST'], 'bar')
    self.assertEqual(env['GOMA_ARBITRARY_TOOLCHAIN_SUPPORT'], 'true')
