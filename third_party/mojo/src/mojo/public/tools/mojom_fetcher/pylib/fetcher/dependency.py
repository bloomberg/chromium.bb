# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


class Dependency(object):
  """Dependency represents an import request from one mojom file to another.
  """
  def __init__(self, repository, importer, imported):
    self._repository = repository
    self._importer_filename = os.path.normpath(importer)
    self._imported_filename = os.path.normpath(imported)

  def __str__(self):
    return str(self.__dict__)

  def __eq__(self, other):
    return self.__dict__ == other.__dict__

  def get_importer(self):
    """Returns the name and full path of the file doing the import."""
    return self._importer_filename

  def get_imported(self):
    """Returns the imported file (filename and path)."""
    return self._imported_filename

  def is_sdk_dep(self):
    """Returns whether this dependency is from the mojo SDK."""
    return (self._imported_filename.startswith("mojo/public/") or
            self._imported_filename.startswith("//mojo/public/"))

  def _is_in_external(self):
    """Returns whether this dependency is under the external directory."""
    common = os.path.commonprefix((self._repository.get_external_directory(),
                                   self._importer_filename))
    return common == self._repository.get_external_directory()

  def maybe_is_a_url(self):
    """Returns whether this dependency may be pointing to a downloadable
    ressource."""
    if self._is_in_external() and not self.is_sdk_dep():
      # External dependencies may refer to other dependencies by relative path,
      # so they can always be URLs.
      return True

    base, _ = self._imported_filename.split(os.path.sep, 1)
    if not '.' in base:
      # There is no dot separator in the first part of the path; it cannot be a
      # URL.
      return False
    return True

  def generate_candidate_urls(self):
    """Generates possible paths where to download this dependency. It is
    expected that at most one of them should work."""
    candidates = []

    base, _ = self._imported_filename.split(os.path.sep, 1)
    if '.' in base and not base.startswith('.'):
      # This import may be an absolute URL path (without scheme).
      candidates.append(self._imported_filename)

    # External dependencies may refer to other dependencies by relative path.
    if self._is_in_external():
      directory = os.path.relpath(os.path.dirname(self._importer_filename),
                                  self._repository.get_external_directory())

      # This is to handle the case where external dependencies use
      # imports relative to a directory upper in the directory structure. As we
      # don't know which directory, we need to go through all of them.
      while len(directory) > 0:
        candidates.append(os.path.join(directory, self._imported_filename))
        directory = os.path.dirname(directory)
    return candidates

  def get_search_path_for_dependency(self):
    """Return all possible search paths for this dependency."""

    # Root directory and external directory are always included.
    search_paths = set([self._repository.get_repo_root_directory(),
                        self._repository.get_external_directory()])
    # Local import paths
    search_paths.add(os.path.dirname(self._importer_filename))

    if self._is_in_external():
      directory = os.path.dirname(self._importer_filename)

      # This is to handle the case where external dependencies use
      # imports relative to a directory upper in the directory structure. As we
      # don't know which directory, we need to go through all of them.
      while self._repository.get_external_directory() in directory:
        search_paths.add(directory)
        directory = os.path.dirname(directory)
    return search_paths
