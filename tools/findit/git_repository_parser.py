# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import xml.dom.minidom as minidom
from xml.parsers.expat import ExpatError

import crash_utils
from repository_parser_interface import ParserInterface

FILE_CHANGE_TYPE_MAP = {
    'add': 'A',
    'copy': 'C',
    'delete': 'D',
    'modify': 'M',
    'rename': 'R'
}


def _ConvertToFileChangeType(file_action):
  # TODO(stgao): verify impact on code that checks the file change type.
  return file_action[0].upper()


class GitParser(ParserInterface):
  """Parser for Git repository in googlesource.

  Attributes:
    parsed_deps: A map from component path to its repository name, regression,
                 etc.
    url_parts_map: A map from url type to its url parts. This parts are added
                   the base url to form different urls.
  """

  def __init__(self, parsed_deps, url_parts_map):
    self.component_to_url_map = parsed_deps
    self.url_parts_map = url_parts_map

  def ParseChangelog(self, component_path, range_start, range_end):
    file_to_revision_map = {}
    revision_map = {}
    base_url = self.component_to_url_map[component_path]['repository']
    changelog_url = base_url + self.url_parts_map['changelog_url']
    revision_url = base_url + self.url_parts_map['revision_url']

    # Retrieve data from the url, return empty maps if fails. Html url is a\
    # url where the changelog can be parsed from html.
    url = changelog_url % (range_start, range_end)
    html_url = url + '?pretty=fuller'
    response = crash_utils.GetDataFromURL(html_url)
    if not response:
      return (revision_map, file_to_revision_map)

    # Parse xml out of the returned string. If it failes, Try parsing
    # from JSON objects.
    try:
      dom = minidom.parseString(response)
    except ExpatError:
      self.ParseChangelogFromJSON(range_start, range_end, changelog_url,
                                  revision_url, revision_map,
                                  file_to_revision_map)
      return (revision_map, file_to_revision_map)

    # The revisions information are in from the third divs to the second
    # to last one.
    divs = dom.getElementsByTagName('div')[2:-1]
    pres = dom.getElementsByTagName('pre')
    uls = dom.getElementsByTagName('ul')

    # Divs, pres and uls each contain revision information for one CL, so
    # they should have same length.
    if not divs or len(divs) != len(pres) or len(pres) != len(uls):
      self.ParseChangelogFromJSON(range_start, range_end, changelog_url,
                                  revision_url, revision_map,
                                  file_to_revision_map)
      return (revision_map, file_to_revision_map)

    # Iterate through divs and parse revisions
    for (div, pre, ul) in zip(divs, pres, uls):
      # Create new revision object for each revision.
      revision = {}

      # There must be three <tr>s. If not, this page is wrong.
      trs = div.getElementsByTagName('tr')
      if len(trs) != 3:
        continue

      # Retrieve git hash.
      githash = trs[0].getElementsByTagName('a')[0].firstChild.nodeValue

      # Retrieve and set author.
      author = trs[1].getElementsByTagName(
          'td')[0].firstChild.nodeValue.split('<')[0]
      revision['author'] = author

      # Retrive and set message.
      revision['message'] = pre.firstChild.nodeValue

      # Set url of this CL.
      revision_url_part = self.url_parts_map['revision_url'] % githash
      revision['url'] = base_url + revision_url_part

      # Go through changed files, they are in li.
      lis = ul.getElementsByTagName('li')
      for li in lis:
        # Retrieve path and action of the changed file
        file_path = li.getElementsByTagName('a')[0].firstChild.nodeValue
        file_change_type = li.getElementsByTagName('span')[
            0].getAttribute('class')

        # Normalize file action so that it is same as SVN parser.
        file_change_type = _ConvertToFileChangeType(file_change_type)

        # Add the changed file to the map.
        if file_path not in file_to_revision_map:
          file_to_revision_map[file_path] = []
        file_to_revision_map[file_path].append((githash, file_change_type))

      # Add this revision object to the map.
      revision_map[githash] = revision

    # Parse one revision for the start range, because googlesource does not
    # include the start of the range.
    self.ParseRevision(revision_url, range_start, revision_map,
                       file_to_revision_map)

    return (revision_map, file_to_revision_map)

  def ParseChangelogFromJSON(self, range_start, range_end, changelog_url,
                             revision_url, revision_map, file_to_revision_map):
    """Parses changelog by going over the JSON file.

    Args:
      range_start: Starting range of the regression.
      range_end: Ending range of the regression.
      changelog_url: The url to retrieve changelog from.
      revision_url: The url to retrieve individual revision from.
      revision_map: A map from a git hash number to its revision information.
      file_to_revision_map: A map from file to a git hash in which it occurs.
    """
    # Compute URLs from given range, and retrieves changelog. Stop if it fails.
    changelog_url %= (range_start, range_end)
    json_url = changelog_url + '?format=json'
    response = crash_utils.GetDataFromURL(json_url)
    if not response:
      return

    # Parse changelog from the returned object. The returned string should
    # start with ")}]'\n", so start from the 6th character.
    revisions = crash_utils.LoadJSON(response[5:])
    if not revisions:
      return

    # Parse individual revision in the log.
    for revision in revisions['log']:
      githash = revision['commit']
      self.ParseRevision(revision_url, githash, revision_map,
                         file_to_revision_map)

    # Parse the revision with range_start, because googlesource ignores
    # that one.
    self.ParseRevision(revision_url, range_start, revision_map,
                       file_to_revision_map)

  def ParseRevision(self, revision_url, githash, revision_map,
                    file_to_revision_map):

    # Retrieve data from the URL, return if it fails.
    url = revision_url % githash
    response = crash_utils.GetDataFromURL(url + '?format=json')
    if not response:
      return

    # Load JSON object from the string. If it fails, terminate the function.
    json_revision = crash_utils.LoadJSON(response[5:])
    if not json_revision:
      return

    # Create a map representing object and get githash from the JSON object.
    revision = {}
    githash = json_revision['commit']

    # Set author, message and URL of this CL.
    revision['author'] = json_revision['author']['name']
    revision['message'] = json_revision['message']
    revision['url'] = url

    # Iterate through the changed files.
    for diff in json_revision['tree_diff']:
      file_path = diff['new_path']
      file_change_type = diff['type']

      # Normalize file action so that it fits with svn_repository_parser.
      file_change_type = _ConvertToFileChangeType(file_change_type)

      # Add the file to the map.
      if file_path not in file_to_revision_map:
        file_to_revision_map[file_path] = []
      file_to_revision_map[file_path].append((githash, file_change_type))

    # Add this CL to the map.
    revision_map[githash] = revision

    return

  def ParseLineDiff(self, path, component, file_change_type, githash):
    changed_line_numbers = []
    changed_line_contents = []
    base_url = self.component_to_url_map[component]['repository']
    backup_url = (base_url + self.url_parts_map['revision_url']) % githash

    # If the file is added (not modified), treat it as if it is not changed.
    if file_change_type in ('A', 'C', 'R'):
      # TODO(stgao): Maybe return whole file change for Add, Rename, and Copy?
      return (backup_url, changed_line_numbers, changed_line_contents)

    # Retrieves the diff data from URL, and if it fails, return emptry lines.
    url = (base_url + self.url_parts_map['diff_url']) % (githash, path)
    data = crash_utils.GetDataFromURL(url + '?format=text')
    if not data:
      return (backup_url, changed_line_numbers, changed_line_contents)

    # Decode the returned object to line diff info
    diff = base64.b64decode(data).splitlines()

    # Iterate through the lines in diff. Set current line to -1 so that we know
    # that current line is part of the diff chunk.
    current_line = -1
    for line in diff:
      line = line.strip()

      # If line starts with @@, a new chunk starts.
      if line.startswith('@@'):
        current_line = int(line.split('+')[1].split(',')[0])

      # If we are in a chunk.
      elif current_line != -1:
        # If line is either added or modified.
        if line.startswith('+'):
          changed_line_numbers.append(current_line)
          changed_line_contents.append(line[2:])

        # Do not increment current line if the change is 'delete'.
        if not line.startswith('-'):
          current_line += 1

    # Return url without '?format=json'
    return (url, changed_line_numbers, changed_line_contents)

  def ParseBlameInfo(self, component, file_path, line, revision):
    base_url = self.component_to_url_map[component]['repository']

    # Retrieve blame JSON file from googlesource. If it fails, return None.
    url_part = self.url_parts_map['blame_url'] % (revision, file_path)
    blame_url = base_url + url_part
    json_string = crash_utils.GetDataFromURL(blame_url)
    if not json_string:
      return

    # Parse JSON object from the string. The returned string should
    # start with ")}]'\n", so start from the 6th character.
    annotation = crash_utils.LoadJSON(json_string[5:])
    if not annotation:
      return

    # Go through the regions, which is a list of consecutive lines with same
    # author/revision.
    for blame_line in annotation['regions']:
      start = blame_line['start']
      count = blame_line['count']

      # For each region, check if the line we want the blame info of is in this
      # region.
      if start <= line and line <= start + count - 1:
        # If we are in the right region, get the information from the line.
        revision = blame_line['commit']
        author = blame_line['author']['name']
        revision_url_parts = self.url_parts_map['revision_url'] % revision
        revision_url = base_url + revision_url_parts
        # TODO(jeun): Add a way to get content from JSON object.
        content = None

        (revision_info, _) = self.ParseChangelog(component, revision, revision)
        message = revision_info[revision]['message']
        return (content, revision, author, revision_url, message)

    # Return none if the region does not exist.
    return None
