# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS image pusher (from cbuildbot to signer).

This pushes files from the archive bucket to the signer bucket and marks
artifacts for signing (which a signing process will look for).
"""

from __future__ import print_function

import ConfigParser
import cStringIO
import errno
import getpass
import os
import re
import tempfile
import textwrap

from chromite.buildbot import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import signing


# This will split a fully qualified ChromeOS version string up.
# R34-5126.0.0 will break into "34" and "5126.0.0".
VERSION_REGEX = r'^R([0-9]+)-([^-]+)'

# The test signers will scan this dir looking for test work.
# Keep it in sync with the signer config files [gs_test_buckets].
TEST_SIGN_BUCKET_BASE = 'gs://chromeos-throw-away-bucket/signer-tests'

# Ketsets that are only valid in the above test bucket.
TEST_KEYSETS = set(('test-keys-mp', 'test-keys-premp'))


class MissingBoardInstructions(Exception):
  """Raised when a board lacks any signer instructions."""


class InputInsns(object):
  """Object to hold settings for a signable board.

  Note: The format of the instruction file pushimage outputs (and the signer
  reads) is not exactly the same as the instruction file pushimage reads.
  """

  def __init__(self, board):
    self.board = board

    config = ConfigParser.ConfigParser()
    config.readfp(open(self.GetInsnFile('DEFAULT')))
    try:
      input_insn = self.GetInsnFile('recovery')
      config.readfp(open(input_insn))
    except IOError as e:
      if e.errno == errno.ENOENT:
        # This board doesn't have any signing instructions.
        # This is normal for new or experimental boards.
        raise MissingBoardInstructions(input_insn)
      raise
    self.cfg = config

  def GetInsnFile(self, image_type):
    """Find the signer instruction files for this board/image type.

    Args:
      image_type: The type of instructions to load.  It can be a common file
        (like "DEFAULT"), or one of the --sign-types.

    Returns:
      Full path to the instruction file using |image_type| and |self.board|.
    """
    if image_type == image_type.upper():
      name = image_type
    elif image_type == 'recovery':
      name = self.board
    else:
      name = '%s.%s' % (self.board, image_type)

    return os.path.join(signing.INPUT_INSN_DIR, '%s.instructions' % name)

  @staticmethod
  def SplitCfgField(val):
    """Split a string into multiple elements.

    This centralizes our convention for multiple elements in the input files
    being delimited by either a space or comma.

    Args:
      val: The string to split.

    Returns:
      The list of elements from having done split the string.
    """
    return val.replace(',', ' ').split()

  def GetChannels(self):
    """Return the list of channels to sign for this board.

    If the board-specific config doesn't specify a preference, we'll use the
    common settings.
    """
    return self.SplitCfgField(self.cfg.get('insns', 'channel'))

  def GetKeysets(self):
    """Return the list of keysets to sign for this board."""
    return self.SplitCfgField(self.cfg.get('insns', 'keyset'))

  def OutputInsns(self, image_type, output_file, sect_insns, sect_general):
    """Generate the output instruction file for sending to the signer.

    Note: The format of the instruction file pushimage outputs (and the signer
    reads) is not exactly the same as the instruction file pushimage reads.

    Args:
      image_type: The type of image we will be signing (see --sign-types).
      output_file: The file to write the new instruction file to.
      sect_insns: Items to set/override in the [insns] section.
      sect_general: Items to set/override in the [general] section.
    """
    config = ConfigParser.ConfigParser()
    config.readfp(open(self.GetInsnFile(image_type)))

    # Clear channel entry in instructions file, ensuring we only get
    # one channel for the signer to look at.  Then provide all the
    # other details for this signing request to avoid any ambiguity
    # and to avoid relying on encoding data into filenames.
    for sect, fields in zip(('insns', 'general'), (sect_insns, sect_general)):
      if not config.has_section(sect):
        config.add_section(sect)
      for k, v in fields.iteritems():
        config.set(sect, k, v)

    output = cStringIO.StringIO()
    config.write(output)
    data = output.getvalue()
    osutils.WriteFile(output_file, data)
    cros_build_lib.Debug('generated insns file for %s:\n%s', image_type, data)


def MarkImageToBeSigned(ctx, tbs_base, insns_path, priority):
  """Mark an instructions file for signing.

  This will upload a file to the GS bucket flagging an image for signing by
  the signers.

  Args:
    ctx: A viable gs.GSContext.
    tbs_base: The full path to where the tobesigned directory lives.
    insns_path: The path (relative to |tbs_base|) of the file to sign.
    priority: Set the signing priority (lower == higher prio).

  Returns:
    The full path to the remote tobesigned file.
  """
  if priority < 0 or priority > 99:
    raise ValueError('priority must be [0, 99] inclusive')

  if insns_path.startswith(tbs_base):
    insns_path = insns_path[len(tbs_base):].lstrip('/')

  tbs_path = '%s/tobesigned/%02i,%s' % (tbs_base, priority,
                                        insns_path.replace('/', ','))

  with tempfile.NamedTemporaryFile(
      bufsize=0, prefix='pushimage.tbs.') as temp_tbs_file:
    lines = [
        'PROG=%s' % __file__,
        'USER=%s' % getpass.getuser(),
        'HOSTNAME=%s' % cros_build_lib.GetHostName(fully_qualified=True),
        'GIT_REV=%s' % git.RunGit(constants.CHROMITE_DIR,
                                  ['rev-parse', 'HEAD']).output.rstrip(),
    ]
    osutils.WriteFile(temp_tbs_file.name, '\n'.join(lines) + '\n')
    ctx.Copy(temp_tbs_file.name, tbs_path)

  return tbs_path


def PushImage(src_path, board, versionrev=None, profile=None, priority=50,
              sign_types=None, dry_run=False, mock=False, force_keysets=()):
  """Push the image from the archive bucket to the release bucket.

  Args:
    src_path: Where to copy the files from; can be a local path or gs:// URL.
      Should be a full path to the artifacts in either case.
    board: The board we're uploading artifacts for (e.g. $BOARD).
    versionrev: The full Chromium OS version string (e.g. R34-5126.0.0).
    profile: The board profile in use (e.g. "asan").
    priority: Set the signing priority (lower == higher prio).
    sign_types: If set, a set of types which we'll restrict ourselves to
      signing.  See the --sign-types option for more details.
    dry_run: Show what would be done, but do not upload anything.
    mock: Upload to a testing bucket rather than the real one.
    force_keysets: Set of keysets to use rather than what the inputs say.

  Returns:
    A dictionary that maps 'channel' -> ['gs://signer_instruction_uri1',
                                         'gs://signer_instruction_uri2',
                                         ...]
  """
  if versionrev is None:
    # Extract milestone/version from the directory name.
    versionrev = os.path.basename(src_path)

  # We only support the latest format here.  Older releases can use pushimage
  # from the respective branch which deals with legacy cruft.
  m = re.match(VERSION_REGEX, versionrev)
  if not m:
    raise ValueError('version %s does not match %s' %
                     (versionrev, VERSION_REGEX))
  milestone = m.group(1)
  version = m.group(2)

  # Normalize board to always use dashes not underscores.  This is mostly a
  # historical artifact at this point, but we can't really break it since the
  # value is used in URLs.
  boardpath = board.replace('_', '-')
  if profile is not None:
    boardpath += '-%s' % profile.replace('_', '-')

  ctx = gs.GSContext(dry_run=dry_run)

  try:
    input_insns = InputInsns(board)
  except MissingBoardInstructions as e:
    cros_build_lib.Warning('board "%s" is missing base instruction file: %s',
                           board, e)
    cros_build_lib.Warning('not uploading anything for signing')
    return
  channels = input_insns.GetChannels()

  # We want force_keysets as a set, and keysets as a list.
  force_keysets = set(force_keysets)
  keysets = list(force_keysets) if force_keysets else input_insns.GetKeysets()

  if mock:
    cros_build_lib.Info('Upload mode: mock; signers will not process anything')
    tbs_base = gs_base = os.path.join(constants.TRASH_BUCKET, 'pushimage-tests',
                                      getpass.getuser())
  elif TEST_KEYSETS & force_keysets:
    cros_build_lib.Info('Upload mode: test; signers will process test keys')
    # We need the tbs_base to be in the place the signer will actually scan.
    tbs_base = TEST_SIGN_BUCKET_BASE
    gs_base = os.path.join(tbs_base, getpass.getuser())
  else:
    cros_build_lib.Info('Upload mode: normal; signers will process the images')
    tbs_base = gs_base = constants.RELEASE_BUCKET

  sect_general = {
      'config_board': board,
      'board': boardpath,
      'version': version,
      'versionrev': versionrev,
      'milestone': milestone,
  }
  sect_insns = {}

  if dry_run:
    cros_build_lib.Info('DRY RUN MODE ACTIVE: NOTHING WILL BE UPLOADED')
  cros_build_lib.Info('Signing for channels: %s', ' '.join(channels))
  cros_build_lib.Info('Signing for keysets : %s', ' '.join(keysets))

  instruction_urls = {}

  def _ImageNameBase(image_type=None):
    lmid = ('%s-' % image_type) if image_type else ''
    return 'ChromeOS-%s%s-%s' % (lmid, versionrev, boardpath)

  for channel in channels:
    cros_build_lib.Debug('\n\n#### CHANNEL: %s ####\n', channel)
    sect_insns['channel'] = channel
    sub_path = '%s-channel/%s/%s' % (channel, boardpath, version)
    dst_path = '%s/%s' % (gs_base, sub_path)
    cros_build_lib.Info('Copying images to %s', dst_path)

    recovery_base = _ImageNameBase('recovery')
    factory_base = _ImageNameBase('factory')
    firmware_base = _ImageNameBase('firmware')
    test_base = _ImageNameBase('test')
    hwqual_tarball = 'chromeos-hwqual-%s-%s.tar.bz2' % (board, versionrev)

    # Upload all the files first before flagging them for signing.
    files_to_copy = (
        # <src>                          <dst>
        # <signing type>                 <sfx>
        ('recovery_image.tar.xz',        recovery_base,          'tar.xz',
         'recovery'),

        ('factory_image.zip',            factory_base,           'zip',
         'factory'),

        ('firmware_from_source.tar.bz2', firmware_base,          'tar.bz2',
         'firmware'),

        ('image.zip',                    _ImageNameBase(),       'zip', ''),
        ('chromiumos_test_image.tar.xz', test_base,              'tar.xz', ''),
        ('debug.tgz',                    'debug-%s' % boardpath, 'tgz', ''),
        (hwqual_tarball,                 '', '', ''),
        ('au-generator.zip',             '', '', ''),
    )
    files_to_sign = []
    for src, dst, sfx, image_type in files_to_copy:
      if not dst:
        dst = src
      elif sfx:
        dst += '.%s' % sfx
      try:
        ctx.Copy(os.path.join(src_path, src), os.path.join(dst_path, dst))
      except gs.GSNoSuchKey:
        cros_build_lib.Warning('Skipping %s as it does not exist', src)
        continue

      if image_type:
        dst_base = dst[:-(len(sfx) + 1)]
        assert dst == '%s.%s' % (dst_base, sfx)
        files_to_sign += [[image_type, dst_base, '.%s' % sfx]]

    # Now go through the subset for signing.
    for keyset in keysets:
      cros_build_lib.Debug('\n\n#### KEYSET: %s ####\n', keyset)
      sect_insns['keyset'] = keyset
      for image_type, dst_name, suffix in files_to_sign:
        dst_archive = '%s%s' % (dst_name, suffix)
        sect_general['archive'] = dst_archive
        sect_general['type'] = image_type

        # See if the caller has requested we only sign certain types.
        if sign_types:
          if not image_type in sign_types:
            cros_build_lib.Info('Skipping %s signing as it was not requested',
                                image_type)
            continue
        else:
          # In the default/automatic mode, only flag files for signing if the
          # archives were actually uploaded in a previous stage.
          gs_artifact_path = os.path.join(dst_path, dst_archive)
          if not ctx.Exists(gs_artifact_path):
            cros_build_lib.Info('%s does not exist.  Nothing to sign.',
                                gs_artifact_path)
            continue

        input_insn_path = input_insns.GetInsnFile(image_type)
        if not os.path.exists(input_insn_path):
          cros_build_lib.Info('%s does not exist.  Nothing to sign.',
                              input_insn_path)
          continue

        # Generate the insn file for this artifact that the signer will use,
        # and flag it for signing.
        with tempfile.NamedTemporaryFile(
            bufsize=0, prefix='pushimage.insns.') as insns_path:
          input_insns.OutputInsns(image_type, insns_path.name, sect_insns,
                                  sect_general)

          gs_insns_path = '%s/%s' % (dst_path, dst_name)
          if keyset != keysets[0]:
            gs_insns_path += '-%s' % keyset
          gs_insns_path += '.instructions'

          ctx.Copy(insns_path.name, gs_insns_path)
          MarkImageToBeSigned(ctx, tbs_base, gs_insns_path, priority)
          cros_build_lib.Info('Signing %s image %s', image_type, gs_insns_path)
          instruction_urls.setdefault(channel, []).append(gs_insns_path)

  return instruction_urls


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)

  # The type of image_dir will strip off trailing slashes (makes later
  # processing simpler and the display prettier).
  parser.add_argument('image_dir', default=None, type='local_or_gs_path',
                      help='full path of source artifacts to upload')
  parser.add_argument('--board', default=None, required=True,
                      help='board to generate symbols for')
  parser.add_argument('--profile', default=None,
                      help='board profile in use (e.g. "asan")')
  parser.add_argument('--version', default=None,
                      help='version info (normally extracted from image_dir)')
  parser.add_argument('-n', '--dry-run', default=False, action='store_true',
                      help='show what would be done, but do not upload')
  parser.add_argument('-M', '--mock', default=False, action='store_true',
                      help='upload things to a testing bucket (dev testing)')
  parser.add_argument('--test-sign-mp', default=False, action='store_true',
                      help='mung signing behavior to sign w/test mp keys')
  parser.add_argument('--test-sign-premp', default=False, action='store_true',
                      help='mung signing behavior to sign w/test premp keys')
  parser.add_argument('--priority', type=int, default=50,
                      help='set signing priority (lower == higher prio)')
  parser.add_argument('--sign-types', default=None, nargs='+',
                      choices=('recovery', 'factory', 'firmware'),
                      help='only sign specified image types')
  parser.add_argument('--yes', action='store_true', default=False,
                      help='answer yes to all prompts')

  opts = parser.parse_args(argv)
  opts.Freeze()

  force_keysets = set()
  if opts.test_sign_mp:
    force_keysets.add('test-keys-mp')
  if opts.test_sign_premp:
    force_keysets.add('test-keys-premp')

  # If we aren't using mock or test or dry run mode, then let's prompt the user
  # to make sure they actually want to do this.  It's rare that people want to
  # run this directly and hit the release bucket.
  if not (opts.mock or force_keysets or opts.dry_run) and not opts.yes:
    prolog = '\n'.join(textwrap.wrap(textwrap.dedent(
        'Uploading images for signing to the *release* bucket is not something '
        'you generally should be doing yourself.'), 80)).strip()
    if not cros_build_lib.BooleanPrompt(
        prompt='Are you sure you want to sign these images',
        default=False, prolog=prolog):
      cros_build_lib.Die('better safe than sorry')

  PushImage(opts.image_dir, opts.board, versionrev=opts.version,
            profile=opts.profile, priority=opts.priority,
            sign_types=opts.sign_types, dry_run=opts.dry_run, mock=opts.mock,
            force_keysets=force_keysets)
