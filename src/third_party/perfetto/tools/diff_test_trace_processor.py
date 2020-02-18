#!/usr/bin/env python
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import difflib
import glob
import importlib
import json
import os
import subprocess
import sys
import tempfile

from google.protobuf import descriptor, descriptor_pb2, message_factory
from google.protobuf import reflection, text_format

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def create_metrics_message_factory(metrics_descriptor_path):
  with open(metrics_descriptor_path, 'r') as metrics_descriptor_file:
    metrics_descriptor_content = metrics_descriptor_file.read()

  file_desc_set_pb2 = descriptor_pb2.FileDescriptorSet()
  file_desc_set_pb2.MergeFromString(metrics_descriptor_content)

  desc_by_path = {}
  for f_desc_pb2 in file_desc_set_pb2.file:
    f_desc_pb2_encode = f_desc_pb2.SerializeToString()
    f_desc = descriptor.FileDescriptor(
        name=f_desc_pb2.name,
        package=f_desc_pb2.package,
        serialized_pb=f_desc_pb2_encode)

    for desc in f_desc.message_types_by_name.values():
      desc_by_path[desc.full_name] = desc

  return message_factory.MessageFactory().GetPrototype(
      desc_by_path['perfetto.protos.TraceMetrics'])

def write_diff(expected, actual):
  expected_lines = expected.splitlines(True)
  actual_lines = actual.splitlines(True)
  diff = difflib.unified_diff(expected_lines, actual_lines,
                              fromfile='expected', tofile='actual')
  for line in diff:
    sys.stderr.write(line)

class TestResult(object):
  def __init__(self, test_type, input_name, cmd, expected, actual):
    self.test_type = test_type
    self.input_name = input_name
    self.cmd = cmd
    self.expected = expected
    self.actual = actual

def run_metrics_test(trace_processor_path, gen_trace_path, metric,
                     expected_path, perf_path, metrics_message_factory):
  with open(expected_path, 'r') as expected_file:
    expected = expected_file.read()

  cmd = [
    trace_processor_path,
    '--run-metrics',
    metric,
    '--metrics-output=binary',
    gen_trace_path,
    '--perf-file',
    perf_path,
  ]
  actual = subprocess.check_output(cmd)

  # Expected will be in text proto format and we'll need to parse it to a real
  # proto.
  expected_message = metrics_message_factory()
  text_format.Merge(expected, expected_message)

  # Actual will be the raw bytes of the proto and we'll need to parse it into
  # a message.
  actual_message = metrics_message_factory()
  actual_message.ParseFromString(actual)

  # Convert both back to text format.
  expected_text = text_format.MessageToString(expected_message)
  actual_text = text_format.MessageToString(actual_message)

  return TestResult('metric', metric, cmd, expected_text, actual_text)

def run_query_test(trace_processor_path, gen_trace_path,
                   query_path, expected_path, perf_path):
  with open(expected_path, 'r') as expected_file:
    expected = expected_file.read()

  cmd = [
    trace_processor_path,
    '-q',
    query_path,
    gen_trace_path,
    '--perf-file',
    perf_path,
  ]
  actual = subprocess.check_output(cmd).decode('utf-8')

  return TestResult('query', query_path, cmd, expected, actual)

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--test-type', type=str, default='queries')
  parser.add_argument('--trace-descriptor', type=str)
  parser.add_argument('--metrics-descriptor', type=str)
  parser.add_argument('--perf-file', type=str)
  parser.add_argument('trace_processor', type=str,
                      help='location of trace processor binary')
  args = parser.parse_args()

  test_dir = os.path.join(ROOT_DIR, 'test')
  if args.test_type == 'queries':
    index = os.path.join(test_dir, 'trace_processor', 'index')
  elif args.test_type == 'metrics':
    index = os.path.join(test_dir, 'metrics', 'index')
  else:
    print('Unknown test type {}. Supported: queries, metircs'.format(
      args.test_type))
    return 1

  with open(index, 'r') as file:
    index_lines = file.readlines()

  if args.trace_descriptor:
    trace_descriptor_path = args.trace_descriptor
  else:
    out_path = os.path.dirname(args.trace_processor)
    trace_protos_path = os.path.join(
        out_path, 'gen', 'protos', 'trace')
    trace_descriptor_path = os.path.join(trace_protos_path, 'trace.descriptor')

  if args.metrics_descriptor:
    metrics_descriptor_path = args.metrics_descriptor
  else:
    out_path = os.path.dirname(args.trace_processor)
    metrics_protos_path = os.path.join(
        out_path, 'gen', 'protos', 'perfetto', 'metrics')
    metrics_descriptor_path = os.path.join(metrics_protos_path,
                                           'metrics.descriptor')

  metrics_message_factory = create_metrics_message_factory(
    metrics_descriptor_path)

  perf_data = []
  test_failure = 0
  index_dir = os.path.dirname(index)
  for line in index_lines:
    stripped = line.strip()
    if stripped.startswith('#'):
      continue
    elif not stripped:
      continue

    [trace_fname, query_fname_or_metric, expected_fname] = stripped.split(' ')

    trace_path = os.path.abspath(os.path.join(index_dir, trace_fname))
    expected_path = os.path.abspath(os.path.join(index_dir, expected_fname))
    if not os.path.exists(trace_path):
      print('Trace file not found {}'.format(trace_path))
      return 1
    elif not os.path.exists(expected_path):
      print('Expected file not found {}'.format(expected_path))
      return 1

    if trace_path.endswith('.py'):
      gen_trace_file = tempfile.NamedTemporaryFile()
      python_cmd = ['python', trace_path, trace_descriptor_path]
      subprocess.check_call(python_cmd, stdout=gen_trace_file)
      gen_trace_path = os.path.realpath(gen_trace_file.name)
    else:
      gen_trace_file = None
      gen_trace_path = trace_path

    with tempfile.NamedTemporaryFile() as tmp_perf_file:
      tmp_perf_path = tmp_perf_file.name
      if args.test_type == 'queries':
        query_path = os.path.abspath(
          os.path.join(index_dir, query_fname_or_metric))
        if not os.path.exists(query_path):
          print('Query file not found {}'.format(query_path))
          return 1

        result = run_query_test(args.trace_processor, gen_trace_path,
                                query_path, expected_path, tmp_perf_path)
      elif args.test_type == 'metrics':
        result = run_metrics_test(args.trace_processor, gen_trace_path,
                                  query_fname_or_metric,
                                  expected_path, tmp_perf_path,
                                  metrics_message_factory)
      else:
        assert False

      perf_lines = tmp_perf_file.readlines()

    if gen_trace_file:
      gen_trace_file.close()

    if result.expected == result.actual:
      assert len(perf_lines) == 1
      perf_numbers = perf_lines[0].split(',')

      trace_shortpath = os.path.relpath(trace_path, test_dir)

      assert len(perf_numbers) == 2
      perf_data.append((trace_shortpath, query_fname_or_metric,
                        perf_numbers[0], perf_numbers[1]))
    else:
      sys.stderr.write(
        'Expected did not match actual for trace {} and {} {}\n'
        .format(trace_path, result.test_type, result.input_name))
      sys.stderr.write('Expected file: {}\n'.format(expected_path))
      sys.stderr.write('Command line: {}\n'.format(' '.join(result.cmd)))

      write_diff(result.expected, result.actual)

      test_failure += 1

  if test_failure == 0:
    print('All tests passed successfully')

    if args.perf_file:
      output_data = {
        'benchmarks': [
          {
            'name': '{}|{}'.format(perf_args[0], perf_args[1]),
            'trace_name': perf_args[0],
            'query_name': perf_args[1],
            'ingest_time': perf_args[2],
            'real_time': perf_args[3],
            'time_unit': 'ns',
            'test_type': args.test_type,
          }
          for perf_args in sorted(perf_data)
        ]
      }
      with open(args.perf_file, 'w+') as perf_file:
        perf_file.write(json.dumps(output_data, indent=2))
    return 0
  else:
    print('Total failures: {}'.format(test_failure))
    return 1

if __name__ == '__main__':
  sys.exit(main())
