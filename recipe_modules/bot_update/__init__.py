DEPS = [
  'gclient',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/raw_io',
  'rietveld',
  'recipe_engine/step',
  'tryserver',
]

from recipe_engine.recipe_api import Property
from recipe_engine.types import freeze

PROPERTIES = {
  'mastername': Property(default=None),
  'buildername': Property(default=None),
  'slavename': Property(default=None),
  'issue': Property(default=None),
  'patchset': Property(default=None),
  'patch_url': Property(default=None),
  'patch_project': Property(default=None),
  'repository': Property(default=None),
  'event.patchSet.ref': Property(default=None, param_name="gerrit_ref"),
  'rietveld': Property(default=None),
  'revision': Property(default=None),
  'parent_got_revision': Property(default=None),
  'deps_revision_overrides': Property(default=freeze({})),
  'fail_patch': Property(default=None, kind=str),
}
