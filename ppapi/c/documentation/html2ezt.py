#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''This utility converts the html files as emitted by doxygen into ezt files
that are suitable for inclusion into Google code site.

EZT stands for "EaZy Templating (for Python)".  For more information, see
http://code.google.com/p/ezt/
'''

import optparse
import os
import re
import shutil
import string
import sys
try:
  from BeautifulSoup import BeautifulSoup, Tag
except (ImportError, NotImplementedError):
  print ("This tool requires the BeautifulSoup package "
         "(see http://www.crummy.com/software/BeautifulSoup/).\n"
         "Make sure that the file BeautifulSoup.py is either in this directory "
         "or is available in your PYTHON_PATH")
  raise


class EZTFixer(object):
  '''This class converts the html strings as produced by Doxygen into ezt
  strings as used by the Google code site tools
  '''

  def __init__(self, html):
    self.soup = BeautifulSoup(html)

  def FixTableHeadings(self):
    '''Fixes the doxygen table headings to EZT's liking.

    This includes using <th> instead of <h2> for the heading, and putting
    the "name" attribute into the "id" attribute of the <tr> tag.

    For example, this html:
      <tr><td colspan="2"><h2><a name="pub-attribs"></a>
      Data Fields List</h2></td></tr>

    would be converted to this:
      <tr id="pub-attribs"><th colspan="2">Data Fields List</th></tr>

    Also, this function splits up tables into multiple separate tables if
    a table heading appears in the middle of a table.
    '''

    table_headers = []
    for tag in self.soup.findAll('tr'):
      if tag.td and tag.td.h2 and tag.td.h2.a and tag.td.h2.a['name']:
        tag['id'] = tag.td.h2.a['name']
        tag.td.string = tag.td.h2.a.next
        tag.td.name = 'th'
        table_headers.append(tag)

    # reverse the list so that earlier tags don't delete later tags
    table_headers.reverse()
    # Split up tables that have multiple table header (th) rows
    for tag in table_headers:
      # Is this a heading in the middle of a table?
      if tag.findPreviousSibling('tr') and tag.parent.name == 'table':
        table = tag.parent
        table_parent = table.parent
        table_index = table_parent.contents.index(table)
        new_table = Tag(self.soup, name='table', attrs=table.attrs)
        table_parent.insert(table_index + 1, new_table)
        tag_index = table.contents.index(tag)
        new_table.contents = table.contents[tag_index:]
        del table.contents[tag_index:]

  def RemoveTopHeadings(self):
    '''Removes <div> sections with a header, tabs, or navpath class attribute'''
    header_tags = self.soup.findAll(
        name='div',
        attrs={'class' : re.compile('^(header|tabs[0-9]*|navpath)$')})
    [tag.extract() for tag in header_tags]

  def FixAll(self):
    self.FixTableHeadings()
    self.RemoveTopHeadings()

  def __str__(self):
    return str(self.soup)


def main():
  '''Main entry for the html2ezt utility

  html2ezt takes a list of html files and creates a set of ezt files with
  the same basename and in the same directory as the original html files.
  Each new ezt file contains a file that is suitable for presentation
  on Google Codesite using the EZT tool.'''

  parser = optparse.OptionParser(usage='Usage: %prog [options] files...')

  parser.add_option('-m', '--move', dest='move', action='store_true',
                    default=False, help='move html files to "original_html"')

  options, files = parser.parse_args()

  if not files:
    parser.print_usage()
    return 1

  for filename in files:
    try:
      with open(filename, 'r') as file:
        html = file.read()

      fixer = EZTFixer(html)
      fixer.FixAll()
      new_name = re.sub(re.compile('\.html$'), '.ezt', filename)
      with open(new_name, 'w') as file:
        file.write(str(fixer))
      if options.move:
        new_directory = os.path.join(
            os.path.dirname(os.path.dirname(filename)), 'original_html')
        if not os.path.exists(new_directory):
          os.mkdir(new_directory)
        shutil.move(filename, new_directory)
    except:
      print "Error while processing %s" % filename
      raise

  return 0

if __name__ == '__main__':
  sys.exit(main())
