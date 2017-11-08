# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api


class LuciMigrationApi(recipe_api.RecipeApi):
  """This module assists in migrating builders from Buildbot to pure LUCI stack.

  Finishing migration means you no longer depend on this module.
  """

  def __init__(self, migration_properties, **kwargs):
    super(LuciMigrationApi, self).__init__(**kwargs)
    self._migration_properties = migration_properties

  @property
  def is_luci(self):
    """True if this recipe is currently running on LUCI stack."""
    return bool(self._migration_properties.get('is_luci', False))

  @property
  def is_prod(self):
    """True if this recipe is currently running in production.

    Typical usage is to modify steps which produce external side-effects so that
    non-production runs of the recipe do not affect production data.

    Examples:
      * Uploading to an alternate google storage file name when in non-prod mode
      * Appending a 'non-production' tag to external RPCs
    """
    return bool(self._migration_properties.get('is_prod', True))
