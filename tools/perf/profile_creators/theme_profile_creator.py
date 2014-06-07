# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from profile_creators import extensions_profile_creator


class ThemeProfileCreator(extensions_profile_creator.ExtensionsProfileCreator):
  """Install a theme."""

  def __init__(self):
    super(ThemeProfileCreator, self).__init__()

    # Marc Ecko theme.
    self._theme_to_install = "opjonmehjfmkejjifhhknofdnacklmjk"
