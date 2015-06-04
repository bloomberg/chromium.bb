# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for check_gdata_token_unittest.py."""

from __future__ import print_function

import filecmp
import mox
import os
import shutil

from chromite.lib import cros_build_lib as build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gdata_lib
from chromite.scripts import check_gdata_token as cgt

import gdata.service
from gdata.projecthosting import client as gdata_ph_client
from gdata.spreadsheet import service as gdata_ss_service


# pylint: disable=protected-access


class MainTest(cros_test_lib.MoxOutputTestCase):
  """Test argument handling at the main method level."""

  def testHelp(self):
    """Test that --help is functioning"""
    argv = ['--help']

    with self.OutputCapturer() as output:
      # Running with --help should exit with code==0.
      self.AssertFuncSystemExitZero(cgt.main, argv)

    # Verify that a message beginning with "usage: " was printed.
    stdout = output.GetStdout()
    self.assertTrue(stdout.startswith('usage: '))

  def testMainOutsideChroot(self):
    """Test flow outside chroot"""
    argv = []
    mocked_outsidechroot = self.mox.CreateMock(cgt.OutsideChroot)

    # Create replay script.
    self.mox.StubOutWithMock(build_lib, 'IsInsideChroot')
    self.mox.StubOutWithMock(cgt.OutsideChroot, '__new__')

    build_lib.IsInsideChroot().AndReturn(False)
    cgt.OutsideChroot.__new__(cgt.OutsideChroot, argv).AndReturn(
        mocked_outsidechroot)
    mocked_outsidechroot.Run()
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.main(argv)
    self.mox.VerifyAll()

  def testMainInsideChroot(self):
    """Test flow inside chroot"""
    argv = []
    mocked_insidechroot = self.mox.CreateMock(cgt.InsideChroot)

    # Create replay script.
    self.mox.StubOutWithMock(build_lib, 'IsInsideChroot')
    self.mox.StubOutWithMock(cgt.InsideChroot, '__new__')

    build_lib.IsInsideChroot().AndReturn(True)
    cgt.InsideChroot.__new__(cgt.InsideChroot).AndReturn(mocked_insidechroot)
    mocked_insidechroot.Run()
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.main(argv)
    self.mox.VerifyAll()


class OutsideChrootTest(cros_test_lib.MoxOutputTestCase):
  """Test flow when run outside chroot."""

  def _MockOutsideChroot(self, *args):
    """Prepare mocked OutsideChroot object with |args|."""
    mocked_outsidechroot = self.mox.CreateMock(cgt.OutsideChroot)

    mocked_outsidechroot.args = list(args) if args else []

    return mocked_outsidechroot

  def testOutsideChrootRestartFail(self):
    mocked_outsidechroot = self._MockOutsideChroot()

    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token']
    run_result = cros_test_lib.EasyAttr(returncode=1)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      # Test should exit with failure.
      self.AssertFuncSystemExitNonZero(cgt.OutsideChroot.Run,
                                       mocked_outsidechroot)

    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testOutsideChrootNoTokenFile(self):
    mocked_outsidechroot = self._MockOutsideChroot('foo')

    self.mox.StubOutWithMock(cgt, '_ChrootPathToExternalPath')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token', 'foo']
    run_result = cros_test_lib.EasyAttr(returncode=0)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    cgt._ChrootPathToExternalPath(cgt.TOKEN_FILE).AndReturn('chr-tok')
    os.path.exists('chr-tok').AndReturn(False)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      # Test should exit with failure.
      self.AssertFuncSystemExitNonZero(cgt.OutsideChroot.Run,
                                       mocked_outsidechroot)

    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testOutsideChrootNewTokenFile(self):
    mocked_outsidechroot = self._MockOutsideChroot('foo')

    self.mox.StubOutWithMock(cgt, '_ChrootPathToExternalPath')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(shutil, 'copy2')
    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token', 'foo']
    run_result = cros_test_lib.EasyAttr(returncode=0)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    cgt._ChrootPathToExternalPath(cgt.TOKEN_FILE).AndReturn('chr-tok')
    os.path.exists('chr-tok').AndReturn(True)
    os.path.exists(cgt.TOKEN_FILE).AndReturn(False)
    shutil.copy2('chr-tok', cgt.TOKEN_FILE)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.OutsideChroot.Run(mocked_outsidechroot)
    self.mox.VerifyAll()

  def testOutsideChrootDifferentTokenFile(self):
    mocked_outsidechroot = self._MockOutsideChroot('foo')

    self.mox.StubOutWithMock(cgt, '_ChrootPathToExternalPath')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(shutil, 'copy2')
    self.mox.StubOutWithMock(filecmp, 'cmp')
    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token', 'foo']
    run_result = cros_test_lib.EasyAttr(returncode=0)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    cgt._ChrootPathToExternalPath(cgt.TOKEN_FILE).AndReturn('chr-tok')
    os.path.exists('chr-tok').AndReturn(True)
    os.path.exists(cgt.TOKEN_FILE).AndReturn(True)
    filecmp.cmp(cgt.TOKEN_FILE, 'chr-tok').AndReturn(False)
    shutil.copy2('chr-tok', cgt.TOKEN_FILE)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.OutsideChroot.Run(mocked_outsidechroot)
    self.mox.VerifyAll()

  def testOutsideChrootNoChangeInTokenFile(self):
    mocked_outsidechroot = self._MockOutsideChroot('foo')

    self.mox.StubOutWithMock(cgt, '_ChrootPathToExternalPath')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(filecmp, 'cmp')
    self.mox.StubOutWithMock(build_lib, 'RunCommand')
    cmd = ['check_gdata_token', 'foo']
    run_result = cros_test_lib.EasyAttr(returncode=0)

    # Create replay script.
    build_lib.RunCommand(cmd, enter_chroot=True,
                         print_cmd=False,
                         error_code_ok=True).AndReturn(run_result)
    cgt._ChrootPathToExternalPath(cgt.TOKEN_FILE).AndReturn('chr-tok')
    os.path.exists('chr-tok').AndReturn(True)
    os.path.exists(cgt.TOKEN_FILE).AndReturn(True)
    filecmp.cmp(cgt.TOKEN_FILE, 'chr-tok').AndReturn(True)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.OutsideChroot.Run(mocked_outsidechroot)
    self.mox.VerifyAll()


class InsideChrootTest(cros_test_lib.MoxOutputTestCase):
  """Test flow when run inside chroot."""

  def _MockInsideChroot(self):
    """Prepare mocked OutsideChroot object."""
    mic = self.mox.CreateMock(cgt.InsideChroot)

    mic.creds = self.mox.CreateMock(gdata_lib.Creds)
    mic.gd_client = self.mox.CreateMock(gdata_ss_service.SpreadsheetsService)
    mic.it_client = self.mox.CreateMock(gdata_ph_client.ProjectHostingClient)

    return mic

  def testLoadTokenFile(self):
    mocked_insidechroot = self._MockInsideChroot()

    self.mox.StubOutWithMock(os.path, 'exists')

    # Create replay script
    os.path.exists(cgt.TOKEN_FILE).AndReturn(True)
    mocked_insidechroot.creds.LoadAuthToken(cgt.TOKEN_FILE)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._LoadTokenFile(mocked_insidechroot)
    self.mox.VerifyAll()
    self.assertTrue(result)

  def testSaveTokenFile(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_insidechroot.creds.StoreAuthTokenIfNeeded(cgt.TOKEN_FILE)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.InsideChroot._SaveTokenFile(mocked_insidechroot)
    self.mox.VerifyAll()

  def testLoadTokenFileMissing(self):
    mocked_insidechroot = self._MockInsideChroot()

    self.mox.StubOutWithMock(os.path, 'exists')

    # Create replay script
    os.path.exists(cgt.TOKEN_FILE).AndReturn(False)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._LoadTokenFile(mocked_insidechroot)
    self.mox.VerifyAll()
    self.assertFalse(result)

  def testInsideChrootValidateOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_insidechroot._LoadTokenFile()
    mocked_insidechroot._ValidateTrackerToken().AndReturn(True)
    mocked_insidechroot._ValidateDocsToken().AndReturn(True)
    mocked_insidechroot._SaveTokenFile()
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.InsideChroot.Run(mocked_insidechroot)
    self.mox.VerifyAll()

  def testInsideChrootTrackerValidateFailGenerateOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_insidechroot._LoadTokenFile()
    mocked_insidechroot._ValidateTrackerToken().AndReturn(True)
    mocked_insidechroot._ValidateDocsToken().AndReturn(False)
    mocked_insidechroot._GenerateDocsToken().AndReturn(True)
    mocked_insidechroot._SaveTokenFile()
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.InsideChroot.Run(mocked_insidechroot)
    self.mox.VerifyAll()

  def testInsideChrootDocsValidateFailGenerateOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_insidechroot._LoadTokenFile()
    mocked_insidechroot._ValidateTrackerToken().AndReturn(False)
    mocked_insidechroot._GenerateTrackerToken().AndReturn(True)
    mocked_insidechroot._ValidateDocsToken().AndReturn(True)
    mocked_insidechroot._SaveTokenFile()
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      cgt.InsideChroot.Run(mocked_insidechroot)
    self.mox.VerifyAll()

  def testInsideChrootTrackerValidateFailGenerateFail(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_insidechroot._LoadTokenFile()
    mocked_insidechroot._ValidateTrackerToken().AndReturn(False)
    mocked_insidechroot._GenerateTrackerToken().AndReturn(False)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      # Test should exit with failure.
      self.AssertFuncSystemExitNonZero(cgt.InsideChroot.Run,
                                       mocked_insidechroot)
    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testInsideChrootDocsValidateFailGenerateFail(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_insidechroot._LoadTokenFile()
    mocked_insidechroot._ValidateTrackerToken().AndReturn(True)
    mocked_insidechroot._ValidateDocsToken().AndReturn(False)
    mocked_insidechroot._GenerateDocsToken().AndReturn(False)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      # Test should exit with failure.
      self.AssertFuncSystemExitNonZero(cgt.InsideChroot.Run,
                                       mocked_insidechroot)
    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testGenerateTrackerTokenOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_creds = mocked_insidechroot.creds
    mocked_itclient = mocked_insidechroot.it_client
    mocked_creds.user = 'joe@chromium.org'
    mocked_creds.password = 'shhh'
    auth_token = 'SomeToken'
    mocked_itclient.auth_token = cros_test_lib.EasyAttr(token_string=auth_token)

    mocked_creds.LoadCreds(cgt.CRED_FILE)
    mocked_itclient.ClientLogin(mocked_creds.user, mocked_creds.password,
                                source='Package Status', service='code',
                                account_type='GOOGLE')
    mocked_creds.SetTrackerAuthToken(auth_token)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._GenerateTrackerToken(mocked_insidechroot)
      self.assertTrue(result, '_GenerateTrackerToken should have passed')
    self.mox.VerifyAll()

  def testGenerateTrackerTokenFail(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_creds = mocked_insidechroot.creds
    mocked_itclient = mocked_insidechroot.it_client
    mocked_creds.user = 'joe@chromium.org'
    mocked_creds.password = 'shhh'

    mocked_creds.LoadCreds(cgt.CRED_FILE)
    mocked_itclient.ClientLogin(mocked_creds.user, mocked_creds.password,
                                source='Package Status', service='code',
                                account_type='GOOGLE').AndRaise(
                                    gdata.client.BadAuthentication())
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._GenerateTrackerToken(mocked_insidechroot)
      self.assertFalse(result, '_GenerateTrackerToken should have failed')
    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testValidateTrackerTokenOK(self):
    mocked_insidechroot = self._MockInsideChroot()
    mocked_itclient = mocked_insidechroot.it_client

    self.mox.StubOutWithMock(gdata.gauth.ClientLoginToken, '__new__')

    # Create replay script.
    auth_token = 'SomeToken'
    mocked_insidechroot.creds.tracker_auth_token = auth_token

    gdata.gauth.ClientLoginToken.__new__(gdata.gauth.ClientLoginToken,
                                         auth_token).AndReturn('TokenObj')
    mocked_itclient.get_issues('chromium-os', query=mox.IgnoreArg())
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._ValidateTrackerToken(mocked_insidechroot)
    self.mox.VerifyAll()
    self.assertTrue(result, '_ValidateTrackerToken should have passed')

  def testValidateTrackerTokenFail(self):
    mocked_insidechroot = self._MockInsideChroot()
    mocked_itclient = mocked_insidechroot.it_client

    self.mox.StubOutWithMock(gdata.gauth.ClientLoginToken, '__new__')

    # Create replay script.
    auth_token = 'SomeToken'
    mocked_insidechroot.creds.tracker_auth_token = auth_token

    gdata.gauth.ClientLoginToken.__new__(gdata.gauth.ClientLoginToken,
                                         auth_token).AndReturn('TokenObj')
    mocked_itclient.get_issues('chromium-os', query=mox.IgnoreArg()).AndRaise(
        gdata.client.Error())
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._ValidateTrackerToken(mocked_insidechroot)
      self.assertFalse(result, '_ValidateTrackerToken should have failed')
    self.mox.VerifyAll()

  def testGenerateDocsTokenOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_creds = mocked_insidechroot.creds
    mocked_gdclient = mocked_insidechroot.gd_client
    mocked_creds.user = 'joe@chromium.org'
    mocked_creds.password = 'shhh'
    auth_token = 'SomeToken'

    mocked_creds.LoadCreds(cgt.CRED_FILE)
    mocked_gdclient.ProgrammaticLogin()
    mocked_gdclient.GetClientLoginToken().AndReturn(auth_token)
    mocked_creds.SetDocsAuthToken(auth_token)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._GenerateDocsToken(mocked_insidechroot)
      self.assertTrue(result, '_GenerateDocsToken should have passed')
    self.mox.VerifyAll()

  def testGenerateDocsTokenFail(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    mocked_creds = mocked_insidechroot.creds
    mocked_gdclient = mocked_insidechroot.gd_client
    mocked_creds.user = 'joe@chromium.org'
    mocked_creds.password = 'shhh'

    mocked_creds.LoadCreds(cgt.CRED_FILE)
    mocked_gdclient.ProgrammaticLogin().AndRaise(
        gdata.service.BadAuthentication())
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._GenerateDocsToken(mocked_insidechroot)
      self.assertFalse(result, '_GenerateTrackerToken should have failed')
    self.mox.VerifyAll()

    self.AssertOutputContainsError()

  def testValidateDocsTokenOK(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    auth_token = 'SomeToken'
    mocked_insidechroot.creds.docs_auth_token = auth_token

    mocked_insidechroot.gd_client.SetClientLoginToken(auth_token)
    mocked_insidechroot.gd_client.GetSpreadsheetsFeed()
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._ValidateDocsToken(mocked_insidechroot)
      self.assertTrue(result, '_ValidateDocsToken should have passed')
    self.mox.VerifyAll()

  def testValidateDocsTokenFail(self):
    mocked_insidechroot = self._MockInsideChroot()

    # Create replay script.
    auth_token = 'SomeToken'
    mocked_insidechroot.creds.docs_auth_token = auth_token

    mocked_insidechroot.gd_client.SetClientLoginToken(auth_token)
    expired_error = gdata.service.RequestError({'reason': 'Token expired'})
    mocked_insidechroot.gd_client.GetSpreadsheetsFeed().AndRaise(expired_error)
    self.mox.ReplayAll()

    # Run test verification.
    with self.OutputCapturer():
      result = cgt.InsideChroot._ValidateDocsToken(mocked_insidechroot)
      self.assertFalse(result, '_ValidateDocsToken should have failed')
    self.mox.VerifyAll()
