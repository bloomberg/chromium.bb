# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from profile_creators import extensions_profile_creator


class ThemeProfileCreator(extensions_profile_creator.ExtensionsProfileCreator):
  """Install a theme."""

  def __init__(self):
    # Marc Ecko theme.
    theme_to_install = "opjonmehjfmkejjifhhknofdnacklmjk"
    super(ThemeProfileCreator, self).__init__(theme_to_install=theme_to_install)
