# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import pipes
import subprocess
import sys


COLOR_ANSI_CODE_MAP = {
  'black': 90,
  'red': 91,
  'green': 92,
  'yellow': 93,
  'blue': 94,
  'magenta': 95,
  'cyan': 96,
  'white': 97,
}


def colored(message, color):
  """Wraps the message into ASCII color escape codes.

  Args:
    message: Message to be wrapped.
    color: See COLOR_ANSI_CODE_MAP.keys() for available choices.
  """
  # This only works on Linux and OS X. Windows users must install ANSICON or
  # use VT100 emulation on Windows 10.
  assert color in COLOR_ANSI_CODE_MAP, 'Unsupported color'
  return '\033[%dm%s\033[0m' % (COLOR_ANSI_CODE_MAP[color], message)


def info(message, **kwargs):
  print(message.format(**kwargs))


def comment(message, **kwargs):
  """Prints an import message to the user."""
  print(colored(message.format(**kwargs), 'yellow'))


def fatal(message, **kwargs):
  """Displays an error to the user and terminates the program."""
  error(message, **kwargs)
  sys.exit(1)


def error(message, **kwargs):
  """Displays an error to the user."""
  print(colored(message.format(**kwargs), 'red'))


def step(name):
  """Display a decorated message to the user.

  This is useful to separate major stages of the script. For simple messages,
  please use comment function above.
  """
  boundary = max(80, len(name))
  print(colored('=' * boundary, 'green'))
  print(colored(name, 'green'))
  print(colored('=' * boundary, 'green'))


def ask(question, answers=None, default=None):
  """Asks the user to answer a question with multiple choices.

  Users are able to press Return to access the default answer (if specified) and
  to type part of the full answer, e.g. "y", "ye" or "yes" are all valid answers
  for "yes". The func will ask user again in case an invalid answer is provided.

  Raises ValueError if default is specified, but not listed an a valid answer.

  Args:
    question: Question to be asked.
    answers: List or dictinary describing user choices. In case of a dictionary,
        the keys are the options display to the user and values are the return
        values for this method. In case of a list, returned values are same as
        options displayed to the user. Defaults to {'yes': True, 'no', False}.
    default: Default option chosen on empty answer. Defaults to 'yes' if default
        value is used for answers parameter or to lack of default answer
        otherwise.

  Returns:
    Chosen option from answers. Full option name is returned even if user only
    enters part of it or chooses the default.
  """
  if answers is None:
    answers = {'yes': True, 'no': False}
    default = 'yes'
  if isinstance(answers, list):
    answers = {v: v for v in answers}

  # Generate a set of prefixes for all answers such that the user can type just
  # the minimum number of characters required, e.g. 'y' or 'ye' can be used for
  # the 'yes' answer. Shared prefixes are ignored, e.g. 'n' and 'ne' will not be
  # accepted if 'negate' and 'next' are both valid answers, whereas 'nex' and
  # 'neg' would be accepted.
  inputs = {}
  common_prefixes = set()
  for ans, retval in answers.iteritems():
    for i in range(len(ans)):
      inp = ans[:i+1]
      if inp in inputs:
        common_prefixes.add(inp)
        del inputs[inp]
      if inp not in common_prefixes:
        inputs[inp] = retval

  if default is None:
    prompt = ' [%s] ' % '/'.join(answers)
  elif default in answers:
    ans_with_def = [a if a != default else a.upper()
                    for a in sorted(answers.keys())]
    prompt = ' [%s] ' % '/'.join(ans_with_def)
  else:
    raise ValueError('invalid default answer: "%s"' % default)

  while True:
    print(colored(question + prompt, 'cyan'), end=' ')
    choice = raw_input().strip().lower()
    if default is not None and choice == '':
      return inputs[default]
    elif choice in inputs:
      return inputs[choice]
    else:
      choices = sorted(['"%s"' % a for a in sorted(answers.keys())])
      error('Please respond with %s or %s.' % (
        ', '.join(choices[:-1]), choices[-1]))


def check_log(command, log_path, env=None):
  """Executes a command and writes its stdout to a specified log file.

  On non-zero return value, also prints the content of the file to the screen
  and raises subprocess.CalledProcessError.

  Args:
    command: Command to be run as a list of arguments.
    log_path: Path to a file to which the output will be written.
    env: Environment to run the command in.
  """
  with open(log_path, 'w') as f:
    try:
      cmd_str = ' '.join(command) if isinstance(command, list) else command
      print(colored(cmd_str, 'blue'))
      print(colored('Logging stdout & stderr to %s' % log_path, 'blue'))
      subprocess.check_call(
          command, stdout=f, stderr=subprocess.STDOUT, shell=False, env=env)
    except subprocess.CalledProcessError:
      error('=' * 80)
      error('Received non-zero return code. Log content:')
      error('=' * 80)
      subprocess.call(['cat', log_path])
      error('=' * 80)
      raise


def run(command, ok_fail=False, **kwargs):
  """Prints and runs the command. Allows to ignore non-zero exit code."""
  if not isinstance(command, list):
    raise ValueError('command must be a list')
  print(colored(' '.join(pipes.quote(c) for c in command), 'blue'))
  try:
    return subprocess.check_call(command, **kwargs)
  except subprocess.CalledProcessError:
    if not ok_fail:
      raise
