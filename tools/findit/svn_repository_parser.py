# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import xml.dom.minidom as minidom
from xml.parsers.expat import ExpatError

import crash_utils
from repository_parser_interface import ParserInterface


# This number is 6 because each linediff page in src.chromium.org should
# contain the following tables: table with revision number, table with actual
# diff, table with dropdown menu, table with legend, a border table and a table
# containing page information.
NUM_TABLES_IN_LINEDIFF_PAGE = 6
# Each of the linediff info should contain 3 tds, one for changed line number,
# and two for line contents before/after.
NUM_TDS_IN_LINEDIFF_PAGE = 3


class SVNParser(ParserInterface):
  """Parser for SVN repository using chromium.org, for components in config.

  Attributes:
    url_map: A map from component to the urls, where urls are for changelog,
             revision, line diff and annotation.
  """

  def __init__(self, url_map):
    self.component_to_urls_map = url_map

  def ParseChangelog(self, component, range_start, range_end):
    file_to_revision_map = {}
    revision_map = {}

    # Check if the current component is supported by reading the components
    # parsed from config file. If it is not, fail.

    url_map = self.component_to_urls_map.get(component)
    if not url_map:
      return (revision_map, file_to_revision_map)

    # Retrieve data from the url, return empty map if fails.
    revision_range_str = '%s:%s' % (range_start, range_end)
    url = url_map['changelog_url'] % revision_range_str
    response = crash_utils.GetDataFromURL(url)
    if not response:
      return (revision_map, file_to_revision_map)

    # Parse xml out of the returned string. If it fails, return empty map.
    try:
      xml_revisions = minidom.parseString(response)
    except ExpatError:
      return (revision_map, file_to_revision_map)

    # Iterate through the returned XML object.
    revisions = xml_revisions.getElementsByTagName('logentry')
    for revision in revisions:
      # Create new revision object for each of the revision.
      revision_object = {}

      # Set author of the CL.
      revision_object['author'] = revision.getElementsByTagName(
          'author')[0].firstChild.nodeValue

      # Get the revision number from xml.
      revision_number = int(revision.getAttribute('revision'))

      # Iterate through the changed paths in the CL.
      paths = revision.getElementsByTagName('paths')
      if paths:
        for changed_path in paths[0].getElementsByTagName('path'):
          # Get path and file change type from the xml.
          file_path = changed_path.firstChild.nodeValue
          file_change_type = changed_path.getAttribute('action')

          if file_path.startswith('/trunk/'):
            file_path = file_path[len('/trunk/'):]

          # Add file to the map.
          if file_path not in file_to_revision_map:
            file_to_revision_map[file_path] = []
          file_to_revision_map[file_path].append(
              (revision_number, file_change_type))

      # Set commit message of the CL.
      revision_object['message'] = revision.getElementsByTagName('msg')[
          0].firstChild.nodeValue

      # Set url of this CL.
      revision_url = url_map['revision_url'] % revision_number
      revision_object['url'] = revision_url

      # Add this CL to the revision map.
      revision_map[revision_number] = revision_object

    return (revision_map, file_to_revision_map)

  def ParseLineDiff(self, path, component, file_change_type, revision_number):
    changed_line_numbers = []
    changed_line_contents = []

    url_map = self.component_to_urls_map.get(component)
    if not url_map:
      return (None, None, None)

    # If the file is added (not modified), treat it as if it is not changed.
    backup_url = url_map['revision_url'] % revision_number
    if file_change_type == 'A':
      return (backup_url, changed_line_numbers, changed_line_contents)

    # Retrieve data from the url. If no data is retrieved, return empty lists.
    url = url_map['diff_url'] % (path, revision_number - 1,
                                 revision_number, revision_number)
    data = crash_utils.GetDataFromURL(url)
    if not data:
      return (backup_url, changed_line_numbers, changed_line_contents)

    line_diff_html = minidom.parseString(data)
    tables = line_diff_html.getElementsByTagName('table')
    # If there are not NUM_TABLES tables in the html page, there should be an
    # error in the html page.
    if len(tables) != NUM_TABLES_IN_LINEDIFF_PAGE:
      return (backup_url, changed_line_numbers, changed_line_contents)

    # Diff content is in the second table. Each line of the diff content
    # is in <tr>.
    trs = tables[1].getElementsByTagName('tr')
    prefix_len = len('vc_diff_')

    # Filter trs so that it only contains diff chunk with contents.
    filtered_trs = []
    for tr in trs:
      tr_class = tr.getAttribute('class')

      # Check for the classes of the <tr>s.
      if tr_class:
        tr_class = tr_class[prefix_len:]

        # Do not have to add header.
        if tr_class == 'header' or tr_class == 'chunk_header':
          continue

        # If the class of tr is empty, this page does not have any change.
        if tr_class == 'empty':
          return (backup_url, changed_line_numbers, changed_line_contents)

      filtered_trs.append(tr)

    # Iterate through filtered trs, and grab line diff information.
    for tr in filtered_trs:
      tds = tr.getElementsByTagName('td')

      # If there aren't 3 tds, this line does should not contain line diff.
      if len(tds) != NUM_TDS_IN_LINEDIFF_PAGE:
        continue

      # If line number information is not in hyperlink, ignore this line.
      try:
        line_num = tds[0].getElementsByTagName('a')[0].firstChild.nodeValue
        left_diff_type = tds[1].getAttribute('class')[prefix_len:]
        right_diff_type = tds[2].getAttribute('class')[prefix_len:]
      except IndexError:
        continue

      # Treat the line as modified only if both left and right diff has type
      # changed or both have different change type, and if the change is not
      # deletion.
      if (left_diff_type != right_diff_type) or (
          left_diff_type == 'change' and right_diff_type == 'change'):

        # Check if the line content is not empty.
        try:
          new_line = tds[2].firstChild.nodeValue
        except AttributeError:
          new_line = ''

        if not (left_diff_type == 'remove' and right_diff_type == 'empty'):
          changed_line_numbers.append(int(line_num))
          changed_line_contents.append(new_line.strip())

    return (url, changed_line_numbers, changed_line_contents)

  def ParseBlameInfo(self, component, file_path, line, revision):
    url_map = self.component_to_urls_map.get(component)
    if not url_map:
      return None

    # Retrieve blame data from url, return None if fails.
    url = url_map['blame_url'] % (file_path, revision, revision)
    data = crash_utils.GetDataFromURL(url)
    if not data:
      return None

    blame_html = minidom.parseString(data)

    title = blame_html.getElementsByTagName('title')
    # If the returned html page is an exception page, return None.
    if title[0].firstChild.nodeValue == 'ViewVC Exception':
      return None

    # Each of the blame result is in <tr>.
    blame_results = blame_html.getElementsByTagName('tr')
    try:
      blame_result = blame_results[line]
    except IndexError:
      return None

    # There must be 4 <td> for each <tr>. If not, this page is wrong.
    tds = blame_result.getElementsByTagName('td')
    if len(tds) != 4:
      return None

    # The third <td> has the line content, separated by <span>s. Combine
    # those to get a string of changed line. If it has nothing, the line
    # is empty.
    line_content = ''
    if tds[3].hasChildNodes():
      contents = tds[3].childNodes

      for content in contents:
        # Nodetype 3 means it is text node.
        if content.nodeType == minidom.Node.TEXT_NODE:
          line_content += content.nodeValue
        else:
          line_content += content.firstChild.nodeValue

      line_content = line_content.strip()

    # If the current line has the same author/revision as the previous lines,
    # the result is not shown. Propagate up until we find the line with info.
    while not tds[1].firstChild:
      line -= 1
      blame_result = blame_results[line]
      tds = blame_result.getElementsByTagName('td')
    author = tds[1].firstChild.nodeValue

    # Revision can either be in hyperlink or plain text.
    try:
      revision = tds[2].getElementsByTagName('a')[0].firstChild.nodeValue
    except IndexError:
      revision = tds[2].firstChild.nodeValue

    (revision_info, _) = self.ParseChangelog(component, revision, revision)
    message = revision_info[int(revision)]['message']

    # Return the parsed information.
    revision_url = url_map['revision_url'] % int(revision)
    return (line_content, revision, author, revision_url, message)
