# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS image signer logic

Terminology:
  Keyset:     Set of keys used for signing, similar to a keyring
  Signer:     Object able to sign a type of image: bios, ec, kernel, etc...
  Image:      Fle that can be signed by a Signer

Signing requires an image to sign and its required keys.

A Signer is expected to understand how to take an input and output signed
artifacts using the stored Keychain.

A Keyset consists of keys and signed objects called keyblocks.

Signing Flow:

 Key+------+->Keyset+---+->Signer+->Image Out
           |            |
 Keyblock+-+  Image In+-+
"""

from __future__ import print_function


class BaseSigner(object):
  """Base Signer object."""

  # Override the following lists to enforce key requirements
  _required_keys = ()
  _required_keys_public = ()
  _required_keys_private = ()
  _required_keyblocks = ()

  def CheckKeyset(self, keyset):
    """Returns true if all required keys and keyblocks are in keyset."""
    for k in self._required_keys:
      if k not in keyset.keys:
        return False

    for k in self._required_keys_public:
      if not keyset.KeyExists(k, require_public=True):
        return False

    for k in self._required_keys_private:
      if not keyset.KeyExists(k, require_private=True):
        return False

    for kb in self._required_keyblocks:
      if not keyset.KeyblockExists(kb):
        return False

    return True

  def Sign(self, keyset, input_name, output_name):
    """Sign given input to output. Returns True if success."""
    raise NotImplementedError
