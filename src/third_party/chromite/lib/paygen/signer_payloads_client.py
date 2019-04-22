# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This library manages the interfaces to the signer for update payloads."""

from __future__ import print_function

import binascii
import os
import re
import shutil
import tempfile
import time
import threading

from chromite.lib import chroot_util
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import path_util

from chromite.lib.paygen import filelib
from chromite.lib.paygen import gslock
from chromite.lib.paygen import gspaths


# How long to sleep between polling GS to see if signer results are present.
DELAY_CHECKING_FOR_SIGNER_RESULTS_SECONDS = 10

# Signer priority value, slightly higher than the common value 50.
SIGNER_PRIORITY = 45


class SignerPayloadsClientGoogleStorage(object):
  """This class implements the Google Storage signer interface for payloads."""

  def __init__(self, build, work_dir=None, unique=None, ctx=None):
    """This initializer identifies the build an payload that need signatures.

    Args:
      build: An instance of gspaths.Build that defines the build.
      work_dir: A directory inside the chroot to be used for temporarily
                manipulating files. The directory should be cleaned by the
                caller. If it is not passed, a temporary directory will be
                created.
      unique: Force known 'unique' id. Mostly for unittests.
      ctx: GS Context to use for GS operations.
    """
    self._build = build
    self._ctx = ctx if ctx is not None else gs.GSContext()
    self._work_dir = work_dir or chroot_util.TempDirInChroot()

    build_signing_uri = gspaths.ChromeosReleases.BuildPayloadsSigningUri(
        self._build)

    # Uniquify the directory using our pid/thread-id. This can't collide
    # with other hosts because the build is locked to our host in
    # paygen_build.
    if unique is None:
      unique = '%d-%d' % (os.getpid(), threading.current_thread().ident)

    # This is a partial URI that is extended for a lot of other URIs we use.
    self.signing_base_dir = os.path.join(build_signing_uri, unique)

    self.archive_uri = os.path.join(self.signing_base_dir,
                                    'payload.hash.tar.bz2')

  def _CleanSignerFilesByKeyset(self, hashes, keyset, timeout=600):
    """Helper method that cleans up GS files associated with a single keyset.

    Args:
      hashes: A list of hash values to be signed by the signer in string
              format. They are all expected to be 32 bytes in length.
      keyset: keyset to have the hashes signed with.
      timeout: Timeout for acquiring the lock on the files to clean.

    Raises:
      gslock.LockNotAcquired if we can't get a lock on the data within timeout.
    """
    hash_names = self._CreateHashNames(len(hashes))

    instructions_uri = self._CreateInstructionsURI(keyset)
    request_uri = self._SignerRequestUri(instructions_uri)
    signature_uris = self._CreateSignatureURIs(hash_names, keyset)

    paths = [instructions_uri, request_uri]
    paths += signature_uris
    paths += [s + '.md5' for s in signature_uris]

    end_time = time.time() + timeout

    while True:
      try:
        with gslock.Lock(request_uri + '.lock'):
          for path in paths:
            self._ctx.Remove(path, ignore_missing=True)

          return
      except gslock.LockNotAcquired:
        # If we have timed out.
        if time.time() > end_time:
          raise

        time.sleep(DELAY_CHECKING_FOR_SIGNER_RESULTS_SECONDS)

  def _CleanSignerFiles(self, hashes, keysets):
    """Helper method that cleans up all GS files associated with a signing.

    Safe to call repeatedly.

    Args:
      hashes: A list of hash values to be signed by the signer in string
              format. They are all expected to be 32 bytes in length.
      keysets: list of keysets to have the hashes signed with.

    Raises:
      May raise GSLibError if there is an extraordinary GS problem.
    """
    for keyset in keysets:
      self._CleanSignerFilesByKeyset(hashes, keyset)

    # After all keysets have been cleaned up, clean up the archive.
    self._ctx.Remove(self.signing_base_dir, recursive=True, ignore_missing=True)

  def _CreateInstructionsURI(self, keyset):
    """Construct the URI used to upload a set of instructions.

    Args:
      keyset: name of the keyset contained in this instruction set.

    Returns:
      URI for the given instruction set as a string.
    """
    return os.path.join(self.signing_base_dir,
                        '%s.payload.signer.instructions' % keyset)

  def _CreateHashNames(self, hash_count):
    """Helper method that creates file names for each hash in GS.

    These names are arbitrary, and only used when working with the signer.

    Args:
      hash_count: How many hash names are needed?
    """
    result = []
    for i in xrange(1, hash_count + 1):
      result.append('%d.payload.hash' % i)
    return result

  def _CreateSignatureURIs(self, hash_names, keyset):
    """Helper method that creates URIs for the signature output files.

    These names are the actual URIs the signer will populate with ".bin"
    already included.

    Args:
      hash_names: The list of input_names passed to the signer.
      keyset: Keyset name passed to the signer.

    Returns:
      List of URIs expected back from the signer.
    """
    result = []
    for hash_name in hash_names:
      # Based on the pattern defined in _CreateInstructions.
      expanded_name = '%s.%s.signed.bin' % (hash_name, keyset)
      result.append(os.path.join(self.signing_base_dir, expanded_name))
    return result

  def _CreateArchive(self, archive_file, hashes, hash_names):
    """Take the hash strings and bundle them in the signer request format.

    Take the contents of an array of strings, and put them into a specified
    file in .tar.bz2 format. Each string is named with a specified name in
    the tar file.

    The number of hashes and number of hash_names must be equal. The
    archive_file will be created or overridden as needed. It's up to
    the caller to ensure it's cleaned up.

    Args:
      archive_file: Name of file to put the tar contents into.
      hashes: List of hashes to sign, stored in strings.
      hash_names: File names expected in the signer request.
    """
    try:
      tmp_dir = tempfile.mkdtemp(dir=self._work_dir)

      # Copy hash files into tmp_dir with standard hash names.
      for h, hash_name in zip(hashes, hash_names):
        osutils.WriteFile(os.path.join(tmp_dir, hash_name), h, mode='wb')

      cmd = ['tar', '-cjf', archive_file] + hash_names
      cros_build_lib.RunCommand(
          cmd, redirect_stdout=True, redirect_stderr=True, cwd=tmp_dir)
    finally:
      # Cleanup.
      shutil.rmtree(tmp_dir)

  def _CreateInstructions(self, hash_names, keyset):
    """Create the signing instructions to send to the signer.

    Args:
      hash_names: The names of the hash files in the archive to sign.
      keyset: Which keyset to sign the hashes with. Valid keysets are
              defined on the signer. 'update_signer' is currently valid.

    Returns:
      A string that contains the contents of the instructions to send.
    """

    pattern = """
# Auto-generated instruction file for signing payload hashes.

[insns]
generate_metadata = false
keyset = %(keyset)s
channel = %(channel)s

input_files = %(input_files)s
output_names = @BASENAME@.@KEYSET@.signed

[general]
archive = metadata-disable.instructions
type = update_payload
board = %(board)s

archive = %(archive_name)s

# We reuse version for version rev because we may not know the
# correct versionrev "R24-1.2.3"
version = %(version)s
versionrev = %(version)s
"""

    # foo-channel -> foo
    channel = self._build.channel.replace('-channel', '')

    archive_name = os.path.basename(self.archive_uri)
    input_files = ' '.join(hash_names)

    return pattern % {
        'channel': channel,
        'board': self._build.board,
        'version': self._build.version,
        'archive_name': archive_name,
        'input_files': input_files,
        'keyset': keyset,
    }

  def _SignerRequestUri(self, instructions_uri):
    """Find the URI of the empty file to create to ask the signer to sign."""

    exp = r'^gs://%s/(?P<postbucket>.*)$' % self._build.bucket
    m = re.match(exp, instructions_uri)
    relative_uri = m.group('postbucket')

    return 'gs://%s/tobesigned/%d,%s' % (
        self._build.bucket,
        SIGNER_PRIORITY,
        relative_uri.replace('/', ','))

  def _WaitForSignatures(self, signature_uris, timeout=1800):
    """Wait until all uris exist, or timeout.

    Args:
      signature_uris: list of uris to check for.
      timeout: time in seconds to wait for all uris to be created.

    Returns:
      True if the signatures all exist, or False.
    """
    end_time = time.time() + timeout

    missing_signatures = signature_uris[:]

    while missing_signatures and time.time() < end_time:
      while missing_signatures and self._ctx.Exists(missing_signatures[0]):
        missing_signatures.pop(0)

      if missing_signatures:
        time.sleep(DELAY_CHECKING_FOR_SIGNER_RESULTS_SECONDS)

    # If none are missing, we found them all.
    return not missing_signatures

  def _DownloadSignatures(self, signature_uris):
    """Download the list of URIs to in-memory strings.

    Args:
      signature_uris: List of URIs to download.

    Returns:
      List of signatures in strings.
    """

    results = []
    for uri in signature_uris:
      with tempfile.NamedTemporaryFile(dir=self._work_dir,
                                       delete=False) as sig_file:
        sig_file_name = sig_file.name
      try:
        self._ctx.Copy(uri, sig_file_name)
        results.append(osutils.ReadFile(sig_file_name))
      finally:
        # Cleanup the temp file, in case it's still there.
        if os.path.exists(sig_file_name):
          os.remove(sig_file_name)

    return results

  def GetHashSignatures(self, hashes, keysets=('update_signer',)):
    """Take an arbitrary list of hash files, and get them signed.

    Args:
      hashes: A list of hash values to be signed by the signer in string
              format. They are all expected to be 32 bytes in length.
      keysets: list of keysets to have the hashes signed with. The default
               is almost certainly what you want. These names must match
               valid keysets on the signer.

    Returns:
      A dictionary keyed by hash with a list of signatures in string format.
      The list of signatures will correspond to the list of keysets passed
      in.

      hashes, keysets=['update_signer', 'update_signer-v2'] ->

      { hashes[0] : [sig_update_signer, sig_update_signer-v2], ... }

      Returns None if the process failed.

    Raises:
      Can raise a variety of GSLibError errors in extraordinary conditions.
    """

    try:
      # Hash and signature names.
      hash_names = self._CreateHashNames(len(hashes))

      # Create and upload the archive of hashes to sign.
      with tempfile.NamedTemporaryFile() as archive_file:
        self._CreateArchive(archive_file.name, hashes, hash_names)
        self._ctx.Copy(archive_file.name, self.archive_uri)

      # [sig_uri, ...]
      all_signature_uris = []

      # { hash : [sig_uri, ...], ... }
      hash_signature_uris = dict([(h, []) for h in hashes])

      # Upload one signing instruction file and signing request for
      # each keyset.
      for keyset in keysets:
        instructions_uri = self._CreateInstructionsURI(keyset)

        self._ctx.CreateWithContents(
            instructions_uri,
            self._CreateInstructions(hash_names, keyset))

        # Create signer request file with debug friendly contents.
        self._ctx.CreateWithContents(
            self._SignerRequestUri(instructions_uri),
            cros_build_lib.MachineDetails())

        # Remember which signatures we just requested.
        uris = self._CreateSignatureURIs(hash_names, keyset)

        all_signature_uris += uris
        for h, sig_uri in zip(hashes, uris):
          hash_signature_uris[h].append(sig_uri)

      # Wait for the signer to finish all keysets.
      if not self._WaitForSignatures(all_signature_uris):
        logging.error('Signer request timed out.')
        return None

      # Download the results.
      return [self._DownloadSignatures(hash_signature_uris[h]) for h in hashes]

    finally:
      # Clean up the signature related files from this run.
      self._CleanSignerFiles(hashes, keysets)


class UnofficialSignerPayloadsClient(SignerPayloadsClientGoogleStorage):
  """This class is a payload signer for local and test buckets."""

  def __init__(self, private_key, work_dir=None):
    """A signer that signs an update payload with a given key.

    For example there is no test key that can be picked up by
    gs://chromeos-release-test bucket, so this way we can reliably sign and
    verify a payload when needed. Also it can be used to sign payloads locally
    when needed. delta_generator accepts a --private_key flag that allows
    signing the payload inplace. However, it is better to go through the whole
    process of signing the payload, assuming we don't have access to the signers
    at that time. This allows us the keep the signing process (or at least part
    of it) tested constantly.

    Args:
      private_key: A 2048 bits private key in PEM format for signing.
      work_dir: A directory inside the chroot to be used for temporarily
                manipulating files. The directory should be cleaned by the
                caller. If None is passed, the directory will be created.
    """
    assert private_key, 'No private key in PEM format is passed for signing.'

    self._private_key = private_key

    super(UnofficialSignerPayloadsClient, self).__init__(gspaths.Build(),
                                                         work_dir)

  def ExtractPublicKey(self, public_key):
    """Extracts the public key from the given private key.

    This is useful for verifying the payload signed by an unofficial key.

    Args:
      public_key: The path to the public key which needs to be fullfiled.

    Returns:
      The path to the public key generated inside the work_dir.
    """
    cmd = ['openssl', 'rsa', '-in', self._private_key, '-pubout', '-out',
           public_key]
    cros_build_lib.RunCommand(cmd, redirect_stdout=True,
                              combine_stdout_stderr=True)

  def GetHashSignatures(self, hashes, keysets=('update_signer',)):
    """See SignerPayloadsClientGoogleStorage._GetHashsignatures().

    Instead of waiting for the signers to sign the hashes, we just sign then and
    copy them to the requested files. It doesn't really support keysets at this
    point.

    Args:
      Look at SignerPayloadsClientGoogleStorage.GetHashsignatures()

    Returns:
      Look at SignerPayloadsClientGoogleStorage.GetHashsignatures()
    """
    logging.info('Signing the hashes with unoffical keys.')

    key_path = os.path.join(self._work_dir, 'update_key.pem')
    filelib.Copy(self._private_key, key_path)

    signatures = []
    for h in hashes:
      hash_hex = binascii.hexlify(h)
      hash_file = os.path.join(self._work_dir, 'hash-%s.bin' % hash_hex)
      signature_file = os.path.join(self._work_dir,
                                    'signature-%s.bin' % hash_hex)
      osutils.WriteFile(hash_file, h, mode='wb')

      sign_script = path_util.ToChrootPath(os.path.join(
          constants.SOURCE_ROOT,
          'src/platform/vboot_reference/scripts/image_signing/',
          'sign_official_build.sh'))

      cros_build_lib.RunCommand([sign_script, 'update_payload',
                                 path_util.ToChrootPath(hash_file),
                                 path_util.ToChrootPath(self._work_dir),
                                 path_util.ToChrootPath(signature_file)],
                                enter_chroot=True)

      signatures.append([osutils.ReadFile(signature_file)])

    return signatures
