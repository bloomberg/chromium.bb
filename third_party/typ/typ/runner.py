# Copyright 2014 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import fnmatch
import importlib
import inspect
import json
import os
import pdb
import sys
import unittest
import traceback

from collections import OrderedDict

# This ensures that absolute imports of typ modules will work when
# running typ/runner.py as a script even if typ is not installed.
# We need this entry in addition to the one in __main__.py to ensure
# that typ/runner.py works when invoked via subprocess on windows in
# _spawn_main().
path_to_file = os.path.realpath(__file__)
if path_to_file.endswith('.pyc'):  # pragma: no cover
    path_to_file = path_to_file[:-1]
dir_above_typ = os.path.dirname(os.path.dirname(path_to_file))
if dir_above_typ not in sys.path:  # pragma: no cover
    sys.path.append(dir_above_typ)


from typ import json_results
from typ.arg_parser import ArgumentParser
from typ.host import Host
from typ.pool import make_pool
from typ.stats import Stats
from typ.printer import Printer
from typ.test_case import TestCase as TypTestCase
from typ.version import VERSION


Result = json_results.Result
ResultSet = json_results.ResultSet
ResultType = json_results.ResultType


def main(argv=None, host=None, win_multiprocessing=None, **defaults):
    host = host or Host()
    runner = Runner(host=host)
    if win_multiprocessing is not None:
        runner.win_multiprocessing = win_multiprocessing
    return runner.main(argv, **defaults)


class TestInput(object):

    def __init__(self, name, msg='', timeout=None, expected=None):
        self.name = name
        self.msg = msg
        self.timeout = timeout
        self.expected = expected


class TestSet(object):

    def __init__(self, parallel_tests=None, isolated_tests=None,
                 tests_to_skip=None):

        def promote(tests):
            tests = tests or []
            return [test if isinstance(test, TestInput) else TestInput(test)
                    for test in tests]

        self.parallel_tests = promote(parallel_tests)
        self.isolated_tests = promote(isolated_tests)
        self.tests_to_skip = promote(tests_to_skip)


class WinMultiprocessing(object):
    ignore = 'ignore'
    importable = 'importable'
    spawn = 'spawn'

    values = [ignore, importable, spawn]


class _AddTestsError(Exception):
    pass


class Runner(object):

    def __init__(self, host=None):
        self.args = None
        self.classifier = None
        self.cov = None
        self.context = None
        self.coverage_source = None
        self.host = host or Host()
        self.loader = unittest.loader.TestLoader()
        self.printer = None
        self.setup_fn = None
        self.stats = None
        self.teardown_fn = None
        self.top_level_dir = None
        self.win_multiprocessing = WinMultiprocessing.spawn
        self.final_responses = []

        # initialize self.args to the defaults.
        parser = ArgumentParser(self.host)
        self.parse_args(parser, [])

    def main(self, argv=None, **defaults):
        parser = ArgumentParser(self.host)
        self.parse_args(parser, argv, **defaults)
        if parser.exit_status is not None:
            return parser.exit_status

        try:
            ret, _, _ = self.run()
            return ret
        except KeyboardInterrupt:
            self.print_("interrupted, exiting", stream=self.host.stderr)
            return 130

    def parse_args(self, parser, argv, **defaults):
        for attrname in defaults:
            if not hasattr(self.args, attrname):
                parser.error("Unknown default argument name '%s'" % attrname,
                             bailout=False)
                return
        parser.set_defaults(**defaults)
        self.args = parser.parse_args(args=argv)
        if parser.exit_status is not None:
            return

    def print_(self, msg='', end='\n', stream=None):
        self.host.print_(msg, end, stream=stream)

    def run(self, test_set=None):

        ret = 0
        h = self.host

        if self.args.version:
            self.print_(VERSION)
            return ret, None, None

        should_spawn = self._check_win_multiprocessing()
        if should_spawn:
            return self._spawn(test_set)

        ret = self._set_up_runner()
        if ret:  # pragma: no cover
            return ret, None, None

        find_start = h.time()
        if self.cov:  # pragma: no cover
            self.cov.erase()
            self.cov.start()

        full_results = None
        result_set = ResultSet()

        if not test_set:
            ret, test_set = self.find_tests(self.args)
        find_end = h.time()

        if not ret:
            ret, full_results = self._run_tests(result_set, test_set)

        if self.cov:  # pragma: no cover
            self.cov.stop()
            self.cov.save()
        test_end = h.time()

        trace = self._trace_from_results(result_set)
        if full_results:
            self._summarize(full_results)
            self._write(self.args.write_full_results_to, full_results)
            upload_ret = self._upload(full_results)
            if not ret:
                ret = upload_ret
            reporting_end = h.time()
            self._add_trace_event(trace, 'run', find_start, reporting_end)
            self._add_trace_event(trace, 'discovery', find_start, find_end)
            self._add_trace_event(trace, 'testing', find_end, test_end)
            self._add_trace_event(trace, 'reporting', test_end, reporting_end)
            self._write(self.args.write_trace_to, trace)
            self.report_coverage()
        else:
            upload_ret = 0

        return ret, full_results, trace

    def _check_win_multiprocessing(self):
        wmp = self.win_multiprocessing

        ignore, importable, spawn = WinMultiprocessing.values

        if wmp not in WinMultiprocessing.values:
            raise ValueError('illegal value %s for win_multiprocessing' %
                             wmp)

        h = self.host
        if wmp == ignore and h.platform == 'win32':  # pragma: win32
            raise ValueError('Cannot use WinMultiprocessing.ignore for '
                             'win_multiprocessing when actually running '
                             'on Windows.')

        if wmp == ignore or self.args.jobs == 1:
            return False

        if wmp == importable:
            if self._main_is_importable():
                return False
            raise ValueError('The __main__ module (%s) '  # pragma: no cover
                             'may not be importable' %
                             sys.modules['__main__'].__file__)

        assert wmp == spawn
        return True

    def _main_is_importable(self):  # pragma: untested
        path = sys.modules['__main__'].__file__
        if not path:
            return False
        if path.endswith('.pyc'):
            path = path[:-1]
        if not path.endswith('.py'):
            return False
        if path.endswith('__main__.py'):
            # main modules are not directly importable.
            return False

        path = self.host.realpath(path)
        for d in sys.path:
            if path.startswith(self.host.realpath(d)):
                return True
        return False  # pragma: no cover

    def _spawn(self, test_set):
        # TODO: Handle picklable hooks, rather than requiring them to be None.
        assert self.classifier is None
        assert self.context is None
        assert self.setup_fn is None
        assert self.teardown_fn is None
        assert test_set is None
        h = self.host

        if self.args.write_trace_to:  # pragma: untested
            should_delete_trace = False
        else:
            should_delete_trace = True
            fp = h.mktempfile(delete=False)
            fp.close()
            self.args.write_trace_to = fp.name

        if self.args.write_full_results_to:  # pragma: untested
            should_delete_results = False
        else:
            should_delete_results = True
            fp = h.mktempfile(delete=False)
            fp.close()
            self.args.write_full_results_to = fp.name

        argv = ArgumentParser(h).argv_from_args(self.args)
        ret = h.call_inline([h.python_interpreter, path_to_file] + argv)

        trace = self._read_and_delete(self.args.write_trace_to,
                                      should_delete_trace)
        full_results = self._read_and_delete(self.args.write_full_results_to,
                                             should_delete_results)
        return ret, full_results, trace

    def _set_up_runner(self):
        h = self.host
        args = self.args

        self.stats = Stats(args.status_format, h.time, args.jobs)
        self.printer = Printer(
            self.print_, args.overwrite, args.terminal_width)

        self.top_level_dir = args.top_level_dir
        if not self.top_level_dir:
            if args.tests and h.isdir(args.tests[0]):
                # TODO: figure out what to do if multiple files are
                # specified and they don't all have the same correct
                # top level dir.
                d = h.realpath(h.dirname(args.tests[0]))
                if h.exists(d, '__init__.py'):
                    top_dir = d
                else:
                    top_dir = args.tests[0]
            else:
                top_dir = h.getcwd()
            while h.exists(top_dir, '__init__.py'):
                top_dir = h.dirname(top_dir)
            self.top_level_dir = h.realpath(top_dir)

        h.add_to_path(self.top_level_dir)

        for path in args.path:
            h.add_to_path(path)

        if args.coverage:  # pragma: no cover
            try:
                import coverage
            except ImportError:
                h.print_("Error: coverage is not installed")
                return 1
            source = self.args.coverage_source
            if not source:
                source = [self.top_level_dir] + self.args.path
            self.coverage_source = source
            self.cov = coverage.coverage(source=self.coverage_source,
                                         data_suffix=True)
            self.cov.erase()
        return 0

    def find_tests(self, args):
        test_set = TestSet()

        orig_skip = unittest.skip
        orig_skip_if = unittest.skipIf
        if args.all:
            unittest.skip = lambda reason: lambda x: x
            unittest.skipIf = lambda condition, reason: lambda x: x

        try:
            names = self._name_list_from_args(args)
            classifier = self.classifier or _default_classifier(args)

            for name in names:
                try:
                    self._add_tests_to_set(test_set, args.suffixes,
                                           self.top_level_dir, classifier,
                                           name)
                except (AttributeError, ImportError, SyntaxError) as e:
                    ex_str = traceback.format_exc()
                    self.print_('Failed to load "%s" in find_tests: %s' %
                                (name, e))
                    self.print_('  %s' %
                                '\n  '.join(ex_str.splitlines()))
                    self.print_(ex_str)
                    return 1, None
                except _AddTestsError as e:
                    self.print_(str(e))
                    return 1, None

            # TODO: Add support for discovering setupProcess/teardownProcess?

            shard_index = args.shard_index
            total_shards = args.total_shards
            assert total_shards >= 1
            assert shard_index >= 0 and shard_index < total_shards, (
                'shard_index (%d) must be >= 0 and < total_shards (%d)' %
                (shard_index, total_shards))
            test_set.parallel_tests = _sort_inputs(
                test_set.parallel_tests)[shard_index::total_shards]
            test_set.isolated_tests = _sort_inputs(
                test_set.isolated_tests)[shard_index::total_shards]
            test_set.tests_to_skip = _sort_inputs(
                test_set.tests_to_skip)[shard_index::total_shards]
            return 0, test_set
        finally:
            unittest.skip = orig_skip
            unittest.skipIf = orig_skip_if

    def _name_list_from_args(self, args):
        if args.tests:
            names = args.tests
        elif args.file_list:
            if args.file_list == '-':
                s = self.host.stdin.read()
            else:
                s = self.host.read_text_file(args.file_list)
            names = [line.strip() for line in s.splitlines()]
        else:
            names = [self.top_level_dir]
        return names

    def _add_tests_to_set(self, test_set, suffixes, top_level_dir, classifier,
                          name):
        h = self.host
        loader = self.loader
        add_tests = _test_adder(test_set, classifier)

        if h.isfile(name):
            rpath = h.relpath(name, top_level_dir)
            if rpath.endswith('.py'):
                rpath = rpath[:-3]
            module = rpath.replace(h.sep, '.')
            add_tests(loader.loadTestsFromName(module))
        elif h.isdir(name):
            for suffix in suffixes:
                add_tests(loader.discover(name, suffix, top_level_dir))
        else:
            possible_dir = name.replace('.', h.sep)
            if h.isdir(top_level_dir, possible_dir):
                for suffix in suffixes:
                    path = h.join(top_level_dir, possible_dir)
                    suite = loader.discover(path, suffix, top_level_dir)
                    add_tests(suite)
            else:
                add_tests(loader.loadTestsFromName(name))

        # pylint: disable=no-member
        if hasattr(loader, 'errors') and loader.errors:  # pragma: python3
            # In Python3's version of unittest, loader failures get converted
            # into failed test cases, rather than raising exceptions. However,
            # the errors also get recorded so you can err out immediately.
            raise ImportError(loader.errors)

    def _run_tests(self, result_set, test_set):
        h = self.host

        all_tests = [ti.name for ti in
                     _sort_inputs(test_set.parallel_tests +
                                  test_set.isolated_tests +
                                  test_set.tests_to_skip)]

        if self.args.list_only:
            self.print_('\n'.join(all_tests))
            return 0, None

        self._run_one_set(self.stats, result_set, test_set)

        failed_tests = sorted(json_results.failed_test_names(result_set))
        retry_limit = self.args.retry_limit

        while retry_limit and failed_tests:
            if retry_limit == self.args.retry_limit:
                self.flush()
                self.args.overwrite = False
                self.printer.should_overwrite = False
                self.args.verbose = min(self.args.verbose, 1)

            self.print_('')
            self.print_('Retrying failed tests (attempt #%d of %d)...' %
                        (self.args.retry_limit - retry_limit + 1,
                         self.args.retry_limit))
            self.print_('')

            stats = Stats(self.args.status_format, h.time, 1)
            stats.total = len(failed_tests)
            tests_to_retry = TestSet(isolated_tests=list(failed_tests))
            retry_set = ResultSet()
            self._run_one_set(stats, retry_set, tests_to_retry)
            result_set.results.extend(retry_set.results)
            failed_tests = json_results.failed_test_names(retry_set)
            retry_limit -= 1

        if retry_limit != self.args.retry_limit:
            self.print_('')

        full_results = json_results.make_full_results(self.args.metadata,
                                                      int(h.time()),
                                                      all_tests, result_set)

        return (json_results.exit_code_from_full_results(full_results),
                full_results)

    def _run_one_set(self, stats, result_set, test_set):
        stats.total = (len(test_set.parallel_tests) +
                       len(test_set.isolated_tests) +
                       len(test_set.tests_to_skip))
        self._skip_tests(stats, result_set, test_set.tests_to_skip)
        self._run_list(stats, result_set,
                       test_set.parallel_tests, self.args.jobs)
        self._run_list(stats, result_set,
                       test_set.isolated_tests, 1)

    def _skip_tests(self, stats, result_set, tests_to_skip):
        for test_input in tests_to_skip:
            last = self.host.time()
            stats.started += 1
            self._print_test_started(stats, test_input)
            now = self.host.time()
            result = Result(test_input.name, actual=ResultType.Skip,
                            started=last, took=(now - last), worker=0,
                            expected=[ResultType.Skip],
                            out=test_input.msg)
            result_set.add(result)
            stats.finished += 1
            self._print_test_finished(stats, result)

    def _run_list(self, stats, result_set, test_inputs, jobs):
        h = self.host
        running_jobs = set()

        jobs = min(len(test_inputs), jobs)
        if not jobs:
            return

        child = _Child(self)
        pool = make_pool(h, jobs, _run_one_test, child,
                         _setup_process, _teardown_process)
        try:
            while test_inputs or running_jobs:
                while test_inputs and (len(running_jobs) < self.args.jobs):
                    test_input = test_inputs.pop(0)
                    stats.started += 1
                    pool.send(test_input)
                    running_jobs.add(test_input.name)
                    self._print_test_started(stats, test_input)

                result = pool.get()
                running_jobs.remove(result.name)
                result_set.add(result)
                stats.finished += 1
                self._print_test_finished(stats, result)
            pool.close()
        finally:
            self.final_responses.extend(pool.join())

    def _print_test_started(self, stats, test_input):
        if self.args.quiet:
            # Print nothing when --quiet was passed.
            return

        # If -vvv was passed, print when the test is queued to be run.
        # We don't actually know when the test picked up to run, because
        # that is handled by the child process (where we can't easily
        # print things). Otherwise, only print when the test is started
        # if we know we can overwrite the line, so that we do not
        # get multiple lines of output as noise (in -vvv, we actually want
        # the noise).
        test_start_msg = stats.format() + test_input.name
        if self.args.verbose > 2:
            self.update(test_start_msg + ' queued', elide=False)
        if self.args.overwrite:
            self.update(test_start_msg, elide=(not self.args.verbose))

    def _print_test_finished(self, stats, result):
        stats.add_time()

        assert result.actual in [ResultType.Failure, ResultType.Skip,
                                 ResultType.Pass]
        if result.actual == ResultType.Failure:
            result_str = ' failed'
        elif result.actual == ResultType.Skip:
            result_str = ' was skipped'
        elif result.actual == ResultType.Pass:
            result_str = ' passed'

        if result.unexpected:
            result_str += ' unexpectedly'
        if self.args.timing:
            timing_str = ' %.4fs' % result.took
        else:
            timing_str = ''
        suffix = '%s%s' % (result_str, timing_str)
        out = result.out
        err = result.err
        if result.code:
            if out or err:
                suffix += ':\n'
            self.update(stats.format() + result.name + suffix, elide=False)
            for l in out.splitlines():
                self.print_('  %s' % l)
            for l in err.splitlines():
                self.print_('  %s' % l)
        elif not self.args.quiet:
            if self.args.verbose > 1 and (out or err):
                suffix += ':\n'
            self.update(stats.format() + result.name + suffix,
                        elide=(not self.args.verbose))
            if self.args.verbose > 1:
                for l in out.splitlines():
                    self.print_('  %s' % l)
                for l in err.splitlines():
                    self.print_('  %s' % l)
            if self.args.verbose:
                self.flush()

    def update(self, msg, elide):
        self.printer.update(msg, elide)

    def flush(self):
        self.printer.flush()

    def _summarize(self, full_results):
        num_passes = json_results.num_passes(full_results)
        num_failures = json_results.num_failures(full_results)
        num_skips = json_results.num_skips(full_results)

        if self.args.quiet and num_failures == 0:
            return

        if self.args.timing:
            timing_clause = ' in %.1fs' % (self.host.time() -
                                           self.stats.started_time)
        else:
            timing_clause = ''
        self.update('%d test%s passed%s, %d skipped, %d failure%s.' %
                    (num_passes,
                     '' if num_passes == 1 else 's',
                     timing_clause,
                     num_skips,
                     num_failures,
                     '' if num_failures == 1 else 's'), elide=False)
        self.print_()

    def _read_and_delete(self, path, delete):
        h = self.host
        obj = None
        if h.exists(path):
            contents = h.read_text_file(path)
            if contents:
                obj = json.loads(contents)
            if delete:
                h.remove(path)
        return obj

    def _write(self, path, obj):
        if path:
            self.host.write_text_file(path, json.dumps(obj, indent=2) + '\n')

    def _upload(self, full_results):
        h = self.host
        if not self.args.test_results_server:
            return 0

        url, content_type, data = json_results.make_upload_request(
            self.args.test_results_server, self.args.builder_name,
            self.args.master_name, self.args.test_type,
            full_results)

        try:
            h.fetch(url, data, {'Content-Type': content_type})
            return 0
        except Exception as e:
            h.print_('Uploading the JSON results raised "%s"' % str(e))
            return 1

    def report_coverage(self):
        if self.args.coverage:  # pragma: no cover
            self.host.print_()
            import coverage
            cov = coverage.coverage(data_suffix=True)
            cov.combine()
            cov.report(show_missing=self.args.coverage_show_missing,
                       omit=self.args.coverage_omit)
            if self.args.coverage_annotate:
                cov.annotate(omit=self.args.coverage_omit)

    def _add_trace_event(self, trace, name, start, end):
        event = {
            'name': name,
            'ts': int((start - self.stats.started_time) * 1000000),
            'dur': int((end - start) * 1000000),
            'ph': 'X',
            'pid': self.host.getpid(),
            'tid': 0,
        }
        trace['traceEvents'].append(event)

    def _trace_from_results(self, result_set):
        trace = OrderedDict()
        trace['traceEvents'] = []
        trace['otherData'] = {}
        for m in self.args.metadata:
            k, v = m.split('=')
            trace['otherData'][k] = v

        for result in result_set.results:
            started = int((result.started - self.stats.started_time) * 1000000)
            took = int(result.took * 1000000)
            event = OrderedDict()
            event['name'] = result.name
            event['dur'] = took
            event['ts'] = started
            event['ph'] = 'X'  # "Complete" events
            event['pid'] = result.pid
            event['tid'] = result.worker

            args = OrderedDict()
            args['expected'] = sorted(str(r) for r in result.expected)
            args['actual'] = str(result.actual)
            args['out'] = result.out
            args['err'] = result.err
            args['code'] = result.code
            args['unexpected'] = result.unexpected
            args['flaky'] = result.flaky
            event['args'] = args

            trace['traceEvents'].append(event)
        return trace


def _matches(name, globs):
    return any(fnmatch.fnmatch(name, glob) for glob in globs)


def _default_classifier(args):
    def default_classifier(test_set, test):
        name = test.id()
        if not args.all and _matches(name, args.skip):
            test_set.tests_to_skip.append(TestInput(name,
                                                    'skipped by request'))
        elif _matches(name, args.isolate):
            test_set.isolated_tests.append(TestInput(name))
        else:
            test_set.parallel_tests.append(TestInput(name))
    return default_classifier


def _test_adder(test_set, classifier):
    def add_tests(obj):
        if isinstance(obj, unittest.suite.TestSuite):
            for el in obj:
                add_tests(el)
        elif (obj.id().startswith('unittest.loader.LoadTestsFailure') or
              obj.id().startswith('unittest.loader.ModuleImportFailure')):
            # Access to protected member pylint: disable=W0212
            module_name = obj._testMethodName
            try:
                method = getattr(obj, obj._testMethodName)
                method()
            except Exception as e:
                if 'LoadTests' in obj.id():
                    raise _AddTestsError('%s.load_tests() failed: %s'
                                         % (module_name, str(e)))
                else:
                    raise _AddTestsError(str(e))
        else:
            assert isinstance(obj, unittest.TestCase)
            classifier(test_set, obj)
    return add_tests


class _Child(object):

    def __init__(self, parent):
        self.host = None
        self.worker_num = None
        self.all = parent.args.all
        self.debugger = parent.args.debugger
        self.coverage = parent.args.coverage and parent.args.jobs > 1
        self.coverage_source = parent.coverage_source
        self.dry_run = parent.args.dry_run
        self.loader = parent.loader
        self.passthrough = parent.args.passthrough
        self.context = parent.context
        self.setup_fn = parent.setup_fn
        self.teardown_fn = parent.teardown_fn
        self.context_after_setup = None
        self.top_level_dir = parent.top_level_dir
        self.loaded_suites = {}
        self.cov = None


def _setup_process(host, worker_num, child):
    child.host = host
    child.worker_num = worker_num
    # pylint: disable=protected-access

    if child.coverage:  # pragma: no cover
        import coverage
        child.cov = coverage.coverage(source=child.coverage_source,
                                      data_suffix=True)
        child.cov._warn_no_data = False
        child.cov.start()

    if child.setup_fn:
        child.context_after_setup = child.setup_fn(child, child.context)
    else:
        child.context_after_setup = child.context
    return child


def _teardown_process(child):
    res = None
    e = None
    if child.teardown_fn:
        try:
            res = child.teardown_fn(child, child.context_after_setup)
        except Exception as e:
            pass

    if child.cov:  # pragma: no cover
        child.cov.stop()
        child.cov.save()

    return (child.worker_num, res, e)


def _run_one_test(child, test_input):
    h = child.host
    pid = h.getpid()
    test_name = test_input.name

    start = h.time()

    # It is important to capture the output before loading the test
    # to ensure that
    # 1) the loader doesn't logs something we don't captured
    # 2) neither the loader nor the test case grab a reference to the
    #    uncaptured stdout or stderr that later is used when the test is run.
    # This comes up when using the FakeTestLoader and testing typ itself,
    # but could come up when testing non-typ code as well.
    h.capture_output(divert=not child.passthrough)

    ex_str = ''
    try:
        orig_skip = unittest.skip
        orig_skip_if = unittest.skipIf
        if child.all:
            unittest.skip = lambda reason: lambda x: x
            unittest.skipIf = lambda condition, reason: lambda x: x

        try:
            suite = child.loader.loadTestsFromName(test_name)
        except Exception as e:
            ex_str = ('loadTestsFromName("%s") failed: %s\n%s\n' %
                      (test_name, e, traceback.format_exc()))
            try:
                suite = _load_via_load_tests(child, test_name)
                ex_str += ('\nload_via_load_tests(\"%s\") returned %d tests\n' %
                           (test_name, len(list(suite))))
            except Exception as e:  # pragma: untested
                suite = []
                ex_str += ('\nload_via_load_tests("%s") failed: %s\n%s\n' %
                           (test_name, e, traceback.format_exc()))
    finally:
        unittest.skip = orig_skip
        unittest.skipIf = orig_skip_if

    tests = list(suite)
    if len(tests) != 1:
        err = 'Failed to load "%s" in run_one_test' % test_name
        if ex_str:  # pragma: untested
            err += '\n  ' + '\n  '.join(ex_str.splitlines())

        h.restore_output()
        return Result(test_name, ResultType.Failure, start, 0,
                      child.worker_num, unexpected=True, code=1,
                      err=err, pid=pid)

    test_case = tests[0]
    if isinstance(test_case, TypTestCase):
        test_case.child = child
        test_case.context = child.context_after_setup

    test_result = unittest.TestResult()
    out = ''
    err = ''
    try:
        if child.dry_run:
            pass
        elif child.debugger:  # pragma: no cover
            _run_under_debugger(h, test_case, suite, test_result)
        else:
            suite.run(test_result)
    finally:
        out, err = h.restore_output()

    took = h.time() - start
    return _result_from_test_result(test_result, test_name, start, took, out,
                                    err, child.worker_num, pid)


def _run_under_debugger(host, test_case, suite,
                        test_result):  # pragma: no cover
    # Access to protected member pylint: disable=W0212
    test_func = getattr(test_case, test_case._testMethodName)
    fname = inspect.getsourcefile(test_func)
    lineno = inspect.getsourcelines(test_func)[1] + 1
    dbg = pdb.Pdb(stdout=host.stdout.stream)
    dbg.set_break(fname, lineno)
    dbg.runcall(suite.run, test_result)


def _result_from_test_result(test_result, test_name, start, took, out, err,
                             worker_num, pid):
    flaky = False
    if test_result.failures:
        expected = [ResultType.Pass]
        actual = ResultType.Failure
        code = 1
        unexpected = True
        err = err + test_result.failures[0][1]
    elif test_result.errors:
        expected = [ResultType.Pass]
        actual = ResultType.Failure
        code = 1
        unexpected = True
        err = err + test_result.errors[0][1]
    elif test_result.skipped:
        expected = [ResultType.Skip]
        actual = ResultType.Skip
        err = err + test_result.skipped[0][1]
        code = 0
        unexpected = False
    elif test_result.expectedFailures:
        expected = [ResultType.Failure]
        actual = ResultType.Failure
        code = 1
        err = err + test_result.expectedFailures[0][1]
        unexpected = False
    elif test_result.unexpectedSuccesses:
        expected = [ResultType.Failure]
        actual = ResultType.Pass
        code = 0
        unexpected = True
    else:
        expected = [ResultType.Pass]
        actual = ResultType.Pass
        code = 0
        unexpected = False

    return Result(test_name, actual, start, took, worker_num,
                  expected, unexpected, flaky, code, out, err, pid)


def _load_via_load_tests(child, test_name):
    # If we couldn't import a test directly, the test may be only loadable
    # via unittest's load_tests protocol. See if we can find a load_tests
    # entry point that will work for this test.
    loader = child.loader
    comps = test_name.split('.')
    new_suite = unittest.TestSuite()

    while comps:
        name = '.'.join(comps)
        module = None
        suite = None
        if name not in child.loaded_suites:
            try:
                module = importlib.import_module(name)
            except ImportError:
                pass
            if module:
                suite = loader.loadTestsFromModule(module)
            child.loaded_suites[name] = suite
        suite = child.loaded_suites[name]
        if suite:
            for test_case in suite:
                assert isinstance(test_case, unittest.TestCase)
                if test_case.id() == test_name:  # pragma: untested
                    new_suite.addTest(test_case)
                    break
        comps.pop()
    return new_suite


def _sort_inputs(inps):
    return sorted(inps, key=lambda inp: inp.name)


if __name__ == '__main__':  # pragma: no cover
    sys.modules['__main__'].__file__ = path_to_file
    sys.exit(main(win_multiprocessing=WinMultiprocessing.importable))
