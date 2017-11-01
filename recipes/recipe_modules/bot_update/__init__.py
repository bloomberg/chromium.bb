DEPS = [
  'depot_tools',
  'gclient',
  'gerrit',
  'recipe_engine/context',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/raw_io',
  'recipe_engine/source_manifest',
  'recipe_engine/step',
  'rietveld',
  'tryserver',
]

from recipe_engine.recipe_api import Property

PROPERTIES = {
  # Gerrit patches will have all properties about them prefixed with patch_.
  'patch_issue': Property(default=None),  # TODO(tandrii): add kind=int.
  'patch_set': Property(default=None),  # TODO(tandrii): add kind=int.
  'patch_project': Property(default=None),  # Also used by Rietveld.
  'patch_gerrit_url': Property(default=None),
  'patch_repository_url': Property(default=None),
  'patch_ref': Property(default=None),

  # TODO(tAndrii): remove legacy Gerrit fields.
  # Legacy Gerrit fields.
  'event.patchSet.ref': Property(default=None, param_name='gerrit_ref'),

  # Rietveld-only fields.
  'rietveld': Property(default=None),  # Stores Url of Rietveld server.
  'issue': Property(default=None),
  'patchset': Property(default=None),
  'repository': Property(default=None),

  # Common fields for both systems.
  'deps_revision_overrides': Property(default={}),
  'fail_patch': Property(default=None, kind=str),
  'parent_got_revision': Property(default=None),
  'revision': Property(default=None),
}
