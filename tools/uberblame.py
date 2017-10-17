#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import cgi
import colorsys
import difflib
import random
import os
import re
import subprocess
import sys
import tempfile
import textwrap
import webbrowser


class TokenContext(object):
  """Metadata about a token.

  Attributes:
    row: Row index of the token in the data file.
    column: Column index of the token in the data file.
    token: The token string.
    commit: A Commit object that corresponds to the commit that added
      this token.
  """
  def __init__(self, row, column, token, commit=None):
    self.row = row
    self.column = column
    self.token = token
    self.commit = commit


class Commit(object):
  """Commit data.

  Attributes:
    hash: The commit hash.
    author_name: The author's name.
    author_email: the author's email.
    author_date: The date and time the author created this commit.
    message: The commit message.
    diff: The commit diff.
  """
  def __init__(self, hash, author_name, author_email, author_date, message,
               diff):
    self.hash = hash
    self.author_name = author_name
    self.author_email = author_email
    self.author_date = author_date
    self.message = message
    self.diff = diff


def tokenize_data(data):
  """Tokenizes |data|.

  Args:
    data: String to tokenize.

  Returns:
    A list of TokenContexts.
  """
  contexts = []
  in_identifier = False
  identifier_start = 0
  identifier = ''
  row = 0
  column = 0
  line_contexts = []

  for c in data + '\n':
    if c.isalnum() or c == '_':
      if in_identifier:
        identifier += c
      else:
        in_identifier = True
        identifier_start = column
        identifier = c
    else:
      if in_identifier:
        line_contexts.append(
            TokenContext(row, identifier_start, identifier))
      in_identifier = False
      if not c.isspace():
        line_contexts.append(TokenContext(row, column, c))

    if c == '\n':
      row += 1
      column = 0
      contexts.append(line_contexts)
      line_tokens = []
      line_contexts = []
    else:
      column += 1
  return contexts


def compute_unified_diff(old_tokens, new_tokens):
  """Computes the diff between |old_tokens| and |new_tokens|.

  Args:
    old_tokens: Token strings corresponding to the old data.
    new_tokens: Token strings corresponding to the new data.

  Returns:
    The diff, in unified diff format.
  """
  return difflib.unified_diff(old_tokens, new_tokens, n=0, lineterm='')


def parse_chunk_header_file_range(file_range):
  """Parses a chunk header file range.

  Diff chunk headers have the form:
    @@ -<file-range> +<file-range> @@
  File ranges have the form:
    <start line number>,<number of lines changed>

  Args:
    file_range: A chunk header file range.

  Returns:
    A tuple (range_start, range_end).  The endpoints are adjusted such that
    iterating over [range_start, range_end) will give the changed indices.
  """
  if ',' in file_range:
    file_range_parts = file_range.split(',')
    start = int(file_range_parts[0])
    amount = int(file_range_parts[1])
    if amount == 0:
      return (start, start)
    return (start - 1, start + amount - 1)
  else:
    return (int(file_range) - 1, int(file_range))


def compute_changed_token_indices(previous_tokens, current_tokens):
  """Computes changed and added tokens.

  Args:
    previous_tokens: Tokens corresponding to the old file.
    current_tokens: Tokens corresponding to the new file.

  Returns:
    A tuple (added_tokens, changed_tokens).
      added_tokens: A list of indices into |current_tokens|.
      changed_tokens: A map of indices into |current_tokens| to
        indices into |previous_tokens|.
  """
  prev_file_chunk_end = 0
  prev_patched_chunk_end = 0
  added_tokens = []
  changed_tokens = {}
  for line in compute_unified_diff(previous_tokens, current_tokens):
    if line.startswith("@@"):
      parts = line.split(' ')
      removed = parts[1].lstrip('-')
      removed_start, removed_end = parse_chunk_header_file_range(removed)
      added = parts[2].lstrip('+')
      added_start, added_end = parse_chunk_header_file_range(added)
      for i in range(added_start, added_end):
        added_tokens.append(i)
      for i in range(0, removed_start - prev_patched_chunk_end):
        changed_tokens[prev_file_chunk_end + i] = prev_patched_chunk_end + i
      prev_patched_chunk_end = removed_end
      prev_file_chunk_end = added_end
  for i in range(0, len(previous_tokens) - prev_patched_chunk_end):
    changed_tokens[prev_file_chunk_end + i] = prev_patched_chunk_end + i
  return added_tokens, changed_tokens


def flatten_nested_list(l):
  """Flattens a list and provides a mapping from elements in the list back
    into the nested list.

  Args:
    l: A list of lists.

  Returns:
    A tuple (flattened, index_to_position):
      flattened: The flattened list.
      index_to_position: A list of pairs (r, c) such that
        index_to_position[i] == (r, c); flattened[i] == l[r][c]
  """
  flattened = []
  index_to_position = {}
  r = 0
  c = 0
  for nested_list in l:
    for element in nested_list:
      index_to_position[len(flattened)] = (r, c)
      flattened.append(element)
      c += 1
    r += 1
    c = 0
  return (flattened, index_to_position)


def compute_changed_token_positions(previous_tokens, current_tokens):
  """Computes changed and added token positions.

  Args:
    previous_tokens: A list of lists of token strings.  Lines in the file
      correspond to the nested lists.
    current_tokens: A list of lists of token strings.  Lines in the file
      correspond to the nested lists.

  Returns:
    A tuple (added_token_positions, changed_token_positions):
      added_token_positions: A list of pairs that index into |current_tokens|.
      changed_token_positions: A map from pairs that index into
        |current_tokens| to pairs that index into |previous_tokens|.
  """
  flat_previous_tokens, previous_index_to_position = flatten_nested_list(
      previous_tokens)
  flat_current_tokens, current_index_to_position = flatten_nested_list(
      current_tokens)
  added_indices, changed_indices = compute_changed_token_indices(
      flat_previous_tokens, flat_current_tokens)
  added_token_positions = [current_index_to_position[i] for i in added_indices]
  changed_token_positions = {
      current_index_to_position[current_i]:
      previous_index_to_position[changed_indices[current_i]]
      for current_i in changed_indices
  }
  return (added_token_positions, changed_token_positions)


def parse_chunks_from_diff(diff):
  """Returns a generator of chunk data from a diff.

  Args:
    diff: A list of strings, with each string being a line from a diff
      in unified diff format.

  Returns:
    A generator of tuples (added_lines_start, added_lines_end,
                           removed_lines, removed_lines_start)
  """
  in_chunk = False
  chunk_previous = []
  previous_start = None
  current_start = None
  current_end = None
  for line in diff:
    if line.startswith('@@'):
      if in_chunk:
        yield (current_start, current_end,
               chunk_previous, previous_start)
      parts = line.split(' ')
      previous = parts[1].lstrip('-')
      previous_start, _ = parse_chunk_header_file_range(previous)
      current = parts[2].lstrip('+')
      current_start, current_end = parse_chunk_header_file_range(current)
      in_chunk = True
      chunk_previous = []
    elif in_chunk and line.startswith('-'):
      chunk_previous.append(line[1:])
  if current_start != None:
    yield (current_start, current_end,
           chunk_previous, previous_start)


def should_skip_commit(commit):
  """Decides if |commit| should be skipped when computing the blame.

  Commit 5d4451e deleted all files in the repo except for DEPS.  The
  next commit, 1e7896, brought them back.  This is a hack to skip
  those commits (except for the files they modified).  If we did not
  do this, changes would be incorrectly attributed to 1e7896.

  Args:
    commit: A Commit object.

  Returns:
    A boolean indicating if this commit should be skipped.
  """
  banned_commits = [
      '1e78967ed2f1937b3809c19d91e7dd62d756d307',
      '5d4451ebf298d9d71f716cc0135f465cec41fcd0',
  ]
  if commit.hash not in banned_commits:
    return False
  banned_commits_file_exceptions = [
      'DEPS',
      'chrome/browser/ui/views/file_manager_dialog_browsertest.cc',
  ]
  for line in commit.diff:
    if line.startswith('---') or line.startswith('+++'):
      if line.split(' ')[1] in banned_commits_file_exceptions:
        return False
    elif line.startswith('@@'):
      return True
  assert False


def generate_substrings(file):
  """Generates substrings from a file stream, where substrings are
  separated by '\0'.

  For example, the input:
    'a\0bc\0\0\0d\0'
  would produce the output:
    ['a', 'bc', 'd']

  Args:
    file: A readable file.
  """
  BUF_SIZE = 448  # Experimentally found to be pretty fast.
  data = []
  while True:
    buf = file.read(BUF_SIZE)
    parts = buf.split('\0')
    data.append(parts[0])
    if len(parts) > 1:
      joined = ''.join(data)
      if joined != '':
        yield joined
      for i in range(1, len(parts) - 1):
        if parts[i] != '':
          yield parts[i]
      data = [parts[-1]]
    if len(buf) < BUF_SIZE:
      joined = ''.join(data)
      if joined != '':
        yield joined
      return


def generate_commits(git_log_stdout):
  """Parses git log output into a stream of Commit objects.
  """
  substring_generator = generate_substrings(git_log_stdout)
  while True:
    hash = substring_generator.next()
    author_name = substring_generator.next()
    author_email = substring_generator.next()
    author_date = substring_generator.next()
    message = substring_generator.next()
    diff = substring_generator.next().split('\n')
    yield Commit(hash, author_name, author_email, author_date, message, diff)


def uberblame_aux(file_name, git_log_stdout, data):
  """Computes the uberblame of file |file_name|.

  Args:
    file_name: File to uberblame.
    git_log_stdout: A file object that represents the git log output.
    data: A string containing the data of file |file_name|.

  Returns:
    A tuple (data, blame).
      data: File contents.
      blame: A list of TokenContexts.
  """
  blame = tokenize_data(data)

  blamed_tokens = 0
  total_tokens = len(blame)
  uber_blame = (data, blame[:])

  for commit in generate_commits(git_log_stdout):
    if should_skip_commit(commit):
      continue

    offset = 0
    for (added_lines_start, added_lines_end, removed_lines,
         removed_lines_start) in parse_chunks_from_diff(commit.diff):
      added_lines_start += offset
      added_lines_end += offset
      previous_contexts = [token_lines
                           for line_previous in removed_lines
                           for token_lines in tokenize_data(line_previous)]
      previous_tokens = [
          [context.token for context in contexts]
          for contexts in previous_contexts
      ]
      current_contexts = blame[added_lines_start:added_lines_end]
      current_tokens = [
          [context.token for context in contexts]
          for contexts in current_contexts
      ]
      added_token_positions, changed_token_positions = (
          compute_changed_token_positions(previous_tokens, current_tokens))
      for r, c in added_token_positions:
        current_contexts[r][c].commit = commit
        blamed_tokens += 1
      for r, c in changed_token_positions:
        pr, pc = changed_token_positions[(r, c)]
        previous_contexts[pr][pc] = current_contexts[r][c]

      assert added_lines_start <= added_lines_end <= len(blame)
      current_blame_size = len(blame)
      blame[added_lines_start:added_lines_end] = previous_contexts
      offset += len(blame) - current_blame_size

  assert blame == [] or blame == [[]]
  return uber_blame


def uberblame(file_name, revision):
  """Computes the uberblame of file |file_name|.

  Args:
    file_name: File to uberblame.
    revision: The revision to start the uberblame at.

  Returns:
    A tuple (data, blame).
      data: File contents.
      blame: A list of TokenContexts.
  """
  cmd_git_log = [
      'git',
      'log',
      '--minimal',
      '--no-prefix',
      '--follow',
      '-m',
      '--first-parent',
      '-p',
      '-U0',
      '-z',
      '--format=%x00%H%x00%an%x00%ae%x00%ad%x00%B',
      revision,
      '--',
      file_name
  ]
  git_log = subprocess.Popen(cmd_git_log,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
  data = subprocess.check_output(
      ['git', 'show', '%s:%s' % (revision, file_name)])
  data, blame = uberblame_aux(file_name, git_log.stdout, data)

  _, stderr = git_log.communicate()
  if git_log.returncode != 0:
    raise subprocess.CalledProcessError(git_log.returncode, cmd_git_log, stderr)
  return data, blame


def generate_pastel_color():
  (h, l, s) = (random.uniform(0, 1),
               random.uniform(0.8, 0.9),
               random.uniform(0.5, 1))
  (r, g, b) = colorsys.hls_to_rgb(h, l, s)
  return "#%0.2X%0.2X%0.2X" % (int(r*255), int(g*255), int(b*255))


def visualize_uberblame(data, blame):
  """Creates and displays a web page to visualize |blame|.

  Args:
    data: The data file as returned by uberblame().
    blame: A list of TokenContexts as returned by uberblame().
  """
  # Use the same seed for the color generator on each run so that
  # loading the same blame of the same file twice will result in the
  # same generated HTML page.
  random.seed(0x52937865ec62d1ea)
  html = """\
  <html>
    <head>
      <style>
        body {
          font-family: "Courier New";
        }
        pre {
          display: inline;
        }
        span {
          outline: 1pt solid #00000030;
          outline-offset: -1pt;
          cursor: pointer;
        }
        #linenums {
          text-align: right;
        }
        #file_display {
          position: absolute;
          left: 0;
          top: 0;
          width: 50%%;
          height: 100%%;
          overflow: scroll;
        }
        #commit_display_container {
          position: absolute;
          left: 50%%;
          top: 0;
          width: 50%%;
          height: 100%%;
          overflow: scroll;
        }
      </style>
      <script>
        commit_data = %s;
        function display_commit(hash) {
          var e = document.getElementById("commit_display");
          e.innerHTML = commit_data[hash]
        }
      </script>
    </head>
    <body>
      <div id="file_display">
        <table>
          <tbody>
            <tr>
              <td valign="top" id="linenums">
                <pre>%s</pre>
              </td>
              <td valign="top">
                <pre>%s</pre>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
      <div id="commit_display_container" valign="top">
        <pre id="commit_display" />
      </div>
    </body>
  </html>
  """
  html = textwrap.dedent(html)
  commits = {}
  lines = []
  commit_colors = {}
  blame_index = 0
  blame = [context for contexts in blame for context in contexts]
  row = 0
  lastline = ''
  for line in data.split('\n'):
    lastline = line
    column = 0
    for c in line + '\n':
      if blame_index < len(blame):
        token_context = blame[blame_index]
        if (row == token_context.row and
            column == token_context.column + len(token_context.token)):
          if (blame_index + 1 == len(blame) or
              blame[blame_index].commit.hash !=
              blame[blame_index + 1].commit.hash):
            lines.append('</span>')
          blame_index += 1
      if blame_index < len(blame):
        token_context = blame[blame_index]
        if row == token_context.row and column == token_context.column:
          if (blame_index == 0 or
              blame[blame_index - 1].commit.hash !=
              blame[blame_index].commit.hash):
            hash = token_context.commit.hash
            commits[hash] = token_context.commit
            if hash not in commit_colors:
              commit_colors[hash] = generate_pastel_color()
            color = commit_colors[hash]
            lines.append(
                ('<span style="background-color: %s" ' +
                 'onclick="display_commit(&quot;%s&quot;)">') % (color, hash))
      lines.append(cgi.escape(c))
      column += 1
    row += 1
  commit_data = ['{']
  commit_display_format = """\
    commit: {hash}
    Author: {author_name} <{author_email}>
    Date: {author_date}

    {message}
    """
  commit_display_format = textwrap.dedent(commit_display_format)
  links = re.compile(r'(https?:\/\/\S+)')
  for hash in commits:
    commit = commits[hash]
    commit_display = commit_display_format.format(
        hash=hash,
        author_name=commit.author_name,
        author_email=commit.author_email,
        author_date=commit.author_date,
        message=commit.message,
    )
    commit_display = cgi.escape(commit_display, quote=True)
    commit_display = re.sub(
        links, '<a href=\\"\\1\\">\\1</a>', commit_display)
    commit_display = commit_display.replace('\n', '\\n')
    commit_data.append('"%s": "%s",' % (hash, commit_display))
  commit_data.append('}')
  commit_data = ''.join(commit_data)
  line_nums = range(1, row if lastline.strip() == '' else row + 1)
  line_nums = '\n'.join([str(num) for num in line_nums])
  lines = ''.join(lines)
  return html % (commit_data, line_nums, lines)


def show_visualization(html):
  """Display |html| in a web browser.

  Args:
    html: The contents of the file to display, as a string.
  """
  # Keep the temporary file around so the browser has time to open it.
  # TODO(thomasanderson): spin up a temporary web server to serve this
  # file so we don't have to leak it.
  html_file = tempfile.NamedTemporaryFile(delete=False, suffix='.html')
  html_file.write(html)
  html_file.flush()
  if sys.platform.startswith('linux'):
    # Don't show any messages when starting the browser.
    saved_stdout = os.dup(1)
    saved_stderr = os.dup(2)
    os.close(1)
    os.close(2)
    os.open(os.devnull, os.O_RDWR)
    os.open(os.devnull, os.O_RDWR)
  webbrowser.open('file://' + html_file.name)
  if sys.platform.startswith('linux'):
    os.dup2(saved_stdout, 1)
    os.dup2(saved_stderr, 2)
    os.close(saved_stdout)
    os.close(saved_stderr)


def main():
  parser = argparse.ArgumentParser(
      description='Show what revision last modified each token of a file')
  parser.add_argument('revision', default='HEAD', nargs='?',
                      help='Show only commits starting from a revision.')
  parser.add_argument('file', help='The file to uberblame.')
  args = parser.parse_args()

  data, blame = uberblame(args.file, args.revision)
  html = visualize_uberblame(data, blame)
  show_visualization(html)
  return 0


if __name__ == '__main__':
  sys.exit(main())
