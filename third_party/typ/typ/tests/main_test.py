# Copyright 2014 Dirk Pranke. All rights reserved.
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

import io
import json
import os
import sys
import textwrap

from typ import main
from typ import test_case
from typ import Host
from typ import VERSION
from typ.fakes import test_result_server_fake


is_python3 = bool(sys.version_info.major == 3)

if is_python3:  # pragma: python3
    # pylint: disable=redefined-builtin,invalid-name
    unicode = str

d = textwrap.dedent


PASS_TEST_PY = """
import unittest
class PassingTest(unittest.TestCase):
    def test_pass(self):
        pass
"""


PASS_TEST_FILES = {'pass_test.py': PASS_TEST_PY}


FAIL_TEST_PY = """
import unittest
class FailingTest(unittest.TestCase):
    def test_fail(self):
        self.fail()
"""


FAIL_TEST_FILES = {'fail_test.py': FAIL_TEST_PY}


OUTPUT_TEST_PY = """
import sys
import unittest

class PassTest(unittest.TestCase):
  def test_out(self):
    sys.stdout.write("hello on stdout\\n")
    sys.stdout.flush()

  def test_err(self):
    sys.stderr.write("hello on stderr\\n")

class FailTest(unittest.TestCase):
 def test_out_err_fail(self):
    sys.stdout.write("hello on stdout\\n")
    sys.stdout.flush()
    sys.stderr.write("hello on stderr\\n")
    self.fail()
"""


OUTPUT_TEST_FILES = {'output_test.py': OUTPUT_TEST_PY}


SF_TEST_PY = """
import sys
import unittest

class SkipMethods(unittest.TestCase):
    @unittest.skip('reason')
    def test_reason(self):
        self.fail()

    @unittest.skipIf(True, 'reason')
    def test_skip_if_true(self):
        self.fail()

    @unittest.skipIf(False, 'reason')
    def test_skip_if_false(self):
        self.fail()


class SkipSetup(unittest.TestCase):
    def setUp(self):
        self.skipTest('setup failed')

    def test_notrun(self):
        self.fail()


@unittest.skip('skip class')
class SkipClass(unittest.TestCase):
    def test_method(self):
        self.fail()

class SetupClass(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sys.stdout.write('in setupClass\\n')
        sys.stdout.flush()
        assert False, 'setupClass failed'

    def test_method1(self):
        pass

    def test_method2(self):
        pass

class ExpectedFailures(unittest.TestCase):
    @unittest.expectedFailure
    def test_fail(self):
        self.fail()

    @unittest.expectedFailure
    def test_pass(self):
        pass
"""


SF_TEST_FILES = {'sf_test.py': SF_TEST_PY}


LOAD_TEST_PY = """
import unittest


class BaseTest(unittest.TestCase):
    pass


def method_fail(self):
    self.fail()


def method_pass(self):
    pass


def load_tests(_, _2, _3):
    setattr(BaseTest, "test_fail", method_fail)
    setattr(BaseTest, "test_pass", method_pass)
    suite = unittest.TestSuite()
    suite.addTest(BaseTest("test_fail"))
    suite.addTest(BaseTest("test_pass"))
    return suite
"""

LOAD_TEST_FILES = {'load_test.py': LOAD_TEST_PY}



path_to_main = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    'runner.py')


class TestCli(test_case.MainTestCase):
    prog = [sys.executable, path_to_main]
    files_to_ignore = ['*.pyc']

    def test_bad_arg(self):
        self.check(['--bad-arg'], ret=2, out='',
                   rerr='.*: error: unrecognized arguments: --bad-arg\n')
        self.check(['-help'], ret=2, out='',
                   rerr=(".*: error: argument -h/--help: "
                         "ignored explicit argument 'elp'\n"))

    def test_bad_metadata(self):
        self.check(['--metadata', 'foo'], ret=2, err='',
                   out='Error: malformed --metadata "foo"\n')

    def test_basic(self):
        self.check([], files=PASS_TEST_FILES,
                   ret=0,
                   out=('[1/1] pass_test.PassingTest.test_pass passed\n'
                        '1 test passed, 0 skipped, 0 failures.\n'), err='')

    def test_coverage(self):
        try:
            import coverage  # pylint: disable=W0612
            files = {
                'pass_test.py': PASS_TEST_PY,
                'fail_test.py': FAIL_TEST_PY,
            }
            self.check(['-c', 'pass_test'], files=files, ret=0, err='',
                       out=d("""\
                             [1/1] pass_test.PassingTest.test_pass passed
                             1 test passed, 0 skipped, 0 failures.

                             Name           Stmts   Miss  Cover
                             ----------------------------------
                             fail_test.py       4      4     0%
                             pass_test.py       4      0   100%
                             ----------------------------------
                             TOTAL              8      4    50%
                             """))
        except ImportError:  # pragma: no cover
            # We can never cover this line, since running coverage means
            # that import will succeed.
            self.check(['-c'], files=PASS_TEST_FILES, ret=1,
                       out='Error: coverage is not installed\n', err='')

    def test_debugger(self):
        if sys.version_info.major == 3:  # pragma: python3
            return
        else:  # pragma: python2
            _, out, _, _ = self.check(['-d'], stdin='quit()\n',
                                      files=PASS_TEST_FILES, ret=0, err='')
            self.assertIn('(Pdb) ', out)

    def test_dryrun(self):
        self.check(['-n'], files=PASS_TEST_FILES, ret=0, err='',
                   out=d("""\
                         [1/1] pass_test.PassingTest.test_pass passed
                         1 test passed, 0 skipped, 0 failures.
                         """))

    def test_error(self):
        files = {'err_test.py': d("""\
                                  import unittest
                                  class ErrTest(unittest.TestCase):
                                      def test_err(self):
                                          foo = bar
                                  """)}
        _, out, _, _ = self.check([''], files=files, ret=1, err='')
        self.assertIn('[1/1] err_test.ErrTest.test_err failed unexpectedly',
                      out)
        self.assertIn('0 tests passed, 0 skipped, 1 failure', out)

    def test_fail(self):
        _, out, _, _ = self.check([], files=FAIL_TEST_FILES, ret=1, err='')
        self.assertIn('fail_test.FailingTest.test_fail failed unexpectedly',
                      out)

    def test_fail_then_pass(self):
        files = {'fail_then_pass_test.py': d("""\
            import unittest
            count = 0
            class FPTest(unittest.TestCase):
                def test_count(self):
                    global count
                    count += 1
                    if count == 1:
                        self.fail()
            """)}
        _, out, _, files = self.check(['--retry-limit', '3',
                                       '--write-full-results-to',
                                       'full_results.json'],
                                      files=files, ret=0, err='')
        self.assertIn('Retrying failed tests (attempt #1 of 3)', out)
        self.assertNotIn('Retrying failed tests (attempt #2 of 3)', out)
        self.assertIn('1 test passed, 0 skipped, 0 failures.\n', out)
        results = json.loads(files['full_results.json'])
        self.assertEqual(
            results['tests'][
                'fail_then_pass_test']['FPTest']['test_count']['actual'],
            'FAIL PASS')

    def test_fail_then_skip(self):
        files = {'fail_then_skip_test.py': d("""\
            import unittest
            count = 0
            class FPTest(unittest.TestCase):
                def test_count(self):
                    global count
                    count += 1
                    if count == 1:
                        self.fail()
                    elif count == 2:
                        self.skipTest('')
            """)}
        _, out, _, files = self.check(['--retry-limit', '3',
                                       '--write-full-results-to',
                                       'full_results.json'],
                                      files=files, ret=0, err='')
        self.assertIn('Retrying failed tests (attempt #1 of 3)', out)
        self.assertNotIn('Retrying failed tests (attempt #2 of 3)', out)
        self.assertIn('0 tests passed, 1 skipped, 0 failures.\n', out)
        results = json.loads(files['full_results.json'])
        self.assertEqual(
            results['tests'][
                'fail_then_skip_test']['FPTest']['test_count']['actual'],
            'FAIL SKIP')

    def test_failures_are_not_elided(self):
        _, out, _, _ = self.check(['--terminal-width=20'],
                                  files=FAIL_TEST_FILES, ret=1, err='')
        self.assertIn('[1/1] fail_test.FailingTest.test_fail failed '
                      'unexpectedly:\n', out)

    def test_file_list(self):
        files = PASS_TEST_FILES
        self.check(['-f', '-'], files=files, stdin='pass_test\n', ret=0)
        self.check(['-f', '-'], files=files, stdin='pass_test.PassingTest\n',
                   ret=0)
        self.check(['-f', '-'], files=files,
                   stdin='pass_test.PassingTest.test_pass\n',
                   ret=0)
        files = {'pass_test.py': PASS_TEST_PY,
                 'test_list.txt': 'pass_test.PassingTest.test_pass\n'}
        self.check(['-f', 'test_list.txt'], files=files, ret=0)

    def test_find(self):
        files = PASS_TEST_FILES
        self.check(['-l'], files=files, ret=0,
                   out='pass_test.PassingTest.test_pass\n')
        self.check(['-l', 'pass_test'], files=files, ret=0, err='',
                   out='pass_test.PassingTest.test_pass\n')
        self.check(['-l', 'pass_test.py'], files=files, ret=0, err='',
                   out='pass_test.PassingTest.test_pass\n')
        self.check(['-l', './pass_test.py'], files=files, ret=0, err='',
                   out='pass_test.PassingTest.test_pass\n')
        self.check(['-l', '.'], files=files, ret=0, err='',
                   out='pass_test.PassingTest.test_pass\n')
        self.check(['-l', 'pass_test.PassingTest.test_pass'], files=files,
                   ret=0, err='',
                   out='pass_test.PassingTest.test_pass\n')
        self.check(['-l', '.'], files=files, ret=0, err='',
                   out='pass_test.PassingTest.test_pass\n')

    def test_find_from_subdirs(self):
        files = {
            'foo/__init__.py': '',
            'foo/pass_test.py': PASS_TEST_PY,
            'bar/__init__.py': '',
            'bar/tmp': '',

        }
        self.check(['-l', '../foo/pass_test.py'], files=files, cwd='bar',
                   ret=0, err='',
                   out='foo.pass_test.PassingTest.test_pass\n')
        self.check(['-l', 'foo'], files=files, cwd='bar',
                   ret=0, err='',
                   out='foo.pass_test.PassingTest.test_pass\n')
        self.check(['-l', '--path', '../foo', 'pass_test'],
                   files=files, cwd='bar', ret=0, err='',
                   out='pass_test.PassingTest.test_pass\n')

    def test_help(self):
        self.check(['--help'], ret=0, rout='.*', err='')

    def test_import_failure_missing_file(self):
        _, out, _, _ = self.check(['-l', 'foo'], ret=1, err='')
        self.assertIn('Failed to load "foo" in find_tests', out)
        self.assertIn('No module named foo', out)

    def test_import_failure_missing_package(self):
        files = {'foo.py': d("""\
                             import unittest
                             import package_that_does_not_exist

                             class ImportFailureTest(unittest.TestCase):
                                def test_case(self):
                                    pass
                             """)}
        _, out, _, _ = self.check(['-l', 'foo.py'], files=files, ret=1, err='')
        self.assertIn('Failed to load "foo.py" in find_tests', out)
        self.assertIn('No module named package_that_does_not_exist', out)

    def test_import_failure_no_tests(self):
        files = {'foo.py': 'import unittest'}
        self.check(['-l', 'foo'], files=files, ret=0, err='',
                   out='\n')

    def test_import_failure_syntax_error(self):
        files = {'syn_test.py': d("""\
                             import unittest

                             class SyntaxErrorTest(unittest.TestCase):
                                 def test_syntax_error_in_test(self):
                                     syntax error
                             """)}
        _, out, _, _ = self.check([], files=files, ret=1, err='')
        self.assertIn('Failed to import test module: syn_test', out)
        self.assertIn('SyntaxError: invalid syntax', out)

    def test_interrupt(self):
        files = {'interrupt_test.py': d("""\
                                        import unittest
                                        class Foo(unittest.TestCase):
                                           def test_interrupt(self):
                                               raise KeyboardInterrupt()
                                        """)}
        self.check(['-j', '1'], files=files, ret=130, out='',
                   err='interrupted, exiting\n')

    def test_isolate(self):
        self.check(['--isolate', '*test_pass*'], files=PASS_TEST_FILES, ret=0,
                   out=('[1/1] pass_test.PassingTest.test_pass passed\n'
                        '1 test passed, 0 skipped, 0 failures.\n'), err='')

    def test_load_tests_failure(self):
        files = {'foo_test.py': d("""\
                                  import unittest

                                  def load_tests(_, _2, _3):
                                      raise ValueError('this should fail')
                                  """)}
        _, out, _, _ = self.check([], files=files, ret=1, err='')
        self.assertIn('this should fail', out)

    def test_load_tests_single_worker(self):
        files = LOAD_TEST_FILES
        _, out, _, _ = self.check(['-j', '1', '-v'], files=files, ret=1,
                                  err='')
        self.assertIn('[1/2] load_test.BaseTest.test_fail failed', out)
        self.assertIn('[2/2] load_test.BaseTest.test_pass passed', out)
        self.assertIn('1 test passed, 0 skipped, 1 failure.\n', out)

    def test_load_tests_multiple_workers(self):
        _, out, _, _ = self.check([], files=LOAD_TEST_FILES, ret=1, err='')

        # The output for this test is nondeterministic since we may run
        # two tests in parallel. So, we just test that some of the substrings
        # we care about are present.
        self.assertIn('test_pass passed', out)
        self.assertIn('test_fail failed', out)
        self.assertIn('1 test passed, 0 skipped, 1 failure.\n', out)

    def test_missing_builder_name(self):
        self.check(['--test-results-server', 'localhost'], ret=2,
                   out=('Error: --builder-name must be specified '
                        'along with --test-result-server\n'
                        'Error: --master-name must be specified '
                        'along with --test-result-server\n'
                        'Error: --test-type must be specified '
                        'along with --test-result-server\n'), err='')

    def test_ninja_status_env(self):
        self.check(['-v', 'output_test.PassTest.test_out'],
                   files=OUTPUT_TEST_FILES, aenv={'NINJA_STATUS': 'ns: '},
                   out=d("""\
                         ns: output_test.PassTest.test_out passed
                         1 test passed, 0 skipped, 0 failures.
                         """), err='')

    def test_output_for_failures(self):
        _, out, _, _ = self.check(['output_test.FailTest'],
                                  files=OUTPUT_TEST_FILES,
                                  ret=1, err='')
        self.assertIn('[1/1] output_test.FailTest.test_out_err_fail '
                      'failed unexpectedly:\n'
                      '  hello on stdout\n'
                      '  hello on stderr\n', out)

    def test_quiet(self):
        self.check(['-q'], files=PASS_TEST_FILES, ret=0, err='', out='')

    def test_retry_limit(self):
        _, out, _, _ = self.check(['--retry-limit', '2'],
                                  files=FAIL_TEST_FILES, ret=1, err='')
        self.assertIn('Retrying failed tests', out)
        lines = out.splitlines()
        self.assertEqual(len([l for l in lines
                              if 'test_fail failed unexpectedly:' in l]),
                         3)

    def test_skip(self):
        _, out, _, _ = self.check(['--skip', '*test_fail*'],
                                  files=FAIL_TEST_FILES, ret=0)
        self.assertIn('0 tests passed, 1 skipped, 0 failures.', out)

        files = {'fail_test.py': FAIL_TEST_PY,
                 'pass_test.py': PASS_TEST_PY}
        self.check(['-j', '1', '--skip', '*test_fail*'], files=files, ret=0,
                   out=('[1/2] fail_test.FailingTest.test_fail was skipped\n'
                        '[2/2] pass_test.PassingTest.test_pass passed\n'
                        '1 test passed, 1 skipped, 0 failures.\n'), err='')

        # This tests that we print test_started updates for skipped tests
        # properly. It also tests how overwriting works.
        _, out, _, _ = self.check(['-j', '1', '--overwrite', '--skip',
                                   '*test_fail*'], files=files, ret=0,
                                  err='', universal_newlines=False)

        # We test this string separately and call out.strip() to
        # avoid the trailing \r\n we get on windows, while keeping
        # the \r's elsewhere in the string.
        self.assertMultiLineEqual(
            out.strip(),
            ('[0/2] fail_test.FailingTest.test_fail\r'
             '                                     \r'
             '[1/2] fail_test.FailingTest.test_fail was skipped\r'
             '                                                 \r'
             '[1/2] pass_test.PassingTest.test_pass\r'
             '                                     \r'
             '[2/2] pass_test.PassingTest.test_pass passed\r'
             '                                            \r'
             '1 test passed, 1 skipped, 0 failures.'))

    def test_skips_and_failures(self):
        _, out, _, _ = self.check(['-j', '1', '-v', '-v'], files=SF_TEST_FILES,
                                  ret=1, err='')

        # We do a bunch of assertIn()'s to work around the non-portable
        # tracebacks.
        self.assertIn(('[1/9] sf_test.ExpectedFailures.test_fail failed:\n'
                       '  Traceback '), out)
        self.assertIn(('[2/9] sf_test.ExpectedFailures.test_pass '
                       'passed unexpectedly'), out)
        self.assertIn(('[3/9] sf_test.SetupClass.test_method1 '
                       'failed unexpectedly:\n'
                       '  in setupClass\n'), out)
        self.assertIn(('[4/9] sf_test.SetupClass.test_method2 '
                       'failed unexpectedly:\n'
                       '  in setupClass\n'), out)
        self.assertIn(('[5/9] sf_test.SkipClass.test_method was skipped:\n'
                       '  skip class\n'), out)
        self.assertIn(('[6/9] sf_test.SkipMethods.test_reason was skipped:\n'
                       '  reason\n'), out)
        self.assertIn(('[7/9] sf_test.SkipMethods.test_skip_if_false '
                       'failed unexpectedly:\n'
                       '  Traceback'), out)
        self.assertIn(('[8/9] sf_test.SkipMethods.test_skip_if_true '
                       'was skipped:\n'
                       '  reason\n'
                       '[9/9] sf_test.SkipSetup.test_notrun was skipped:\n'
                       '  setup failed\n'
                       '1 test passed, 4 skipped, 4 failures.\n'), out)

    def test_skip_and_all(self):
        # --all should override --skip
        _, out, _, _ = self.check(['--skip', '*test_pass'],
                                  files=PASS_TEST_FILES, ret=0, err='')
        self.assertIn('0 tests passed, 1 skipped, 0 failures.', out)

        _, out, _, _ = self.check(['--all', '--skip', '*test_pass'],
                                  files=PASS_TEST_FILES, ret=0, err='')
        self.assertIn('1 test passed, 0 skipped, 0 failures.', out)

    def test_skip_decorators_and_all(self):
        _, out, _, _ = self.check(['--all', '-j', '1', '-v', '-v'],
                                  files=SF_TEST_FILES, ret=1, err='')
        self.assertIn('sf_test.SkipClass.test_method failed', out)
        self.assertIn('sf_test.SkipMethods.test_reason failed', out)
        self.assertIn('sf_test.SkipMethods.test_skip_if_true failed', out)
        self.assertIn('sf_test.SkipMethods.test_skip_if_false failed', out)

        # --all does not override explicit calls to skipTest(), only
        # the decorators.
        self.assertIn('sf_test.SkipSetup.test_notrun was skipped', out)

    def test_sharding(self):

        def run(shard_index, total_shards, tests):
            files = {'shard_test.py': textwrap.dedent(
                """\
                import unittest
                class ShardTest(unittest.TestCase):
                    def test_01(self):
                        pass

                    def test_02(self):
                        pass

                    def test_03(self):
                        pass

                    def test_04(self):
                        pass

                    def test_05(self):
                        pass
                """)}
            _, out, _, _ = self.check(
                ['--shard-index', str(shard_index),
                 '--total-shards', str(total_shards),
                 '--jobs', '1'],
                files=files)

            exp_out = ''
            total_tests = len(tests)
            for i, test in enumerate(tests):
                exp_out += ('[%d/%d] shard_test.ShardTest.test_%s passed\n' %
                            (i + 1, total_tests, test))
            exp_out += '%d test%s passed, 0 skipped, 0 failures.\n' % (
                total_tests, "" if total_tests == 1 else "s")
            self.assertEqual(out, exp_out)

        run(0, 1, ['01', '02', '03', '04', '05'])
        run(0, 2, ['01', '03', '05'])
        run(1, 2, ['02', '04'])
        run(0, 6, ['01'])

    def test_subdir(self):
        files = {
            'foo/__init__.py': '',
            'foo/bar/__init__.py': '',
            'foo/bar/pass_test.py': PASS_TEST_PY
        }
        self.check(['foo/bar'], files=files, ret=0, err='',
                   out=d("""\
                         [1/1] foo.bar.pass_test.PassingTest.test_pass passed
                         1 test passed, 0 skipped, 0 failures.
                         """))

    def test_timing(self):
        self.check(['-t'], files=PASS_TEST_FILES, ret=0, err='',
                   rout=(r'\[1/1\] pass_test.PassingTest.test_pass passed '
                         r'\d+.\d+s\n'
                         r'1 test passed in \d+.\d+s, 0 skipped, 0 failures.'))

    def test_test_results_server(self):
        server = test_result_server_fake.start()
        self.assertNotEqual(server, None, 'could not start fake server')

        try:
            self.check(['--test-results-server',
                        'http://%s:%d' % server.server_address,
                        '--master-name', 'fake_master',
                        '--builder-name', 'fake_builder',
                        '--test-type', 'typ_tests',
                        '--metadata', 'foo=bar'],
                       files=PASS_TEST_FILES, ret=0, err='',
                       out=('[1/1] pass_test.PassingTest.test_pass passed\n'
                            '1 test passed, 0 skipped, 0 failures.\n'))

        finally:
            posts = server.stop()

        self.assertEqual(len(posts), 1)
        payload = posts[0][2].decode('utf8')
        self.assertIn('"test_pass": {"actual": "PASS"',
                      payload)
        self.assertTrue(payload.endswith('--\r\n'))
        self.assertNotEqual(server.log.getvalue(), '')

    def test_test_results_server_error(self):
        server = test_result_server_fake.start(code=500)
        self.assertNotEqual(server, None, 'could not start fake server')

        try:
            self.check(['--test-results-server',
                        'http://%s:%d' % server.server_address,
                        '--master-name', 'fake_master',
                        '--builder-name', 'fake_builder',
                        '--test-type', 'typ_tests',
                        '--metadata', 'foo=bar'],
                       files=PASS_TEST_FILES, ret=1, err='',
                       out=('[1/1] pass_test.PassingTest.test_pass passed\n'
                            '1 test passed, 0 skipped, 0 failures.\n'
                            'Uploading the JSON results raised '
                            '"HTTP Error 500: Internal Server Error"\n'))

        finally:
            _ = server.stop()

    def test_test_results_server_not_running(self):
        self.check(['--test-results-server', 'http://localhost:99999',
                    '--master-name', 'fake_master',
                    '--builder-name', 'fake_builder',
                    '--test-type', 'typ_tests',
                    '--metadata', 'foo=bar'],
                   files=PASS_TEST_FILES, ret=1, err='',
                   rout=(r'\[1/1\] pass_test.PassingTest.test_pass passed\n'
                         '1 test passed, 0 skipped, 0 failures.\n'
                         'Uploading the JSON results raised .*\n'))

    def test_verbose_2(self):
        self.check(['-vv', '-j', '1', 'output_test.PassTest'],
                   files=OUTPUT_TEST_FILES, ret=0,
                   out=d("""\
                         [1/2] output_test.PassTest.test_err passed:
                           hello on stderr
                         [2/2] output_test.PassTest.test_out passed:
                           hello on stdout
                         2 tests passed, 0 skipped, 0 failures.
                         """), err='')

    def test_verbose_3(self):
        self.check(['-vvv', '-j', '1', 'output_test.PassTest'],
                   files=OUTPUT_TEST_FILES, ret=0,
                   out=d("""\
                         [0/2] output_test.PassTest.test_err queued
                         [1/2] output_test.PassTest.test_err passed:
                           hello on stderr
                         [1/2] output_test.PassTest.test_out queued
                         [2/2] output_test.PassTest.test_out passed:
                           hello on stdout
                         2 tests passed, 0 skipped, 0 failures.
                         """), err='')

    def test_version(self):
        self.check('--version', ret=0, out=(VERSION + '\n'))

    def test_write_full_results_to(self):
        _, _, _, files = self.check(['--write-full-results-to',
                                     'results.json'], files=PASS_TEST_FILES)
        self.assertIn('results.json', files)
        results = json.loads(files['results.json'])
        self.assertEqual(results['interrupted'], False)
        self.assertEqual(results['path_delimiter'], '.')

        # The time it takes to run the test varies, so we test that
        # we got a single entry greater than zero, but then delete it from
        # the result so we can do an exact match on the rest of the trie.
        result = results['tests']['pass_test']['PassingTest']['test_pass']
        self.assertEqual(len(result['times']), 1)
        self.assertGreater(result['times'][0], 0)
        result.pop('times')
        self.assertEqual(results['tests'],
                         {u'pass_test': {
                             u'PassingTest': {
                                 u'test_pass': {
                                     u'actual': u'PASS',
                                     u'expected': u'PASS',
                                 }
                             }
                         }})

    def test_write_trace_to(self):
        _, _, _, files = self.check(['--write-trace-to', 'trace.json'],
                                    files=PASS_TEST_FILES)
        self.assertIn('trace.json', files)
        trace_obj = json.loads(files['trace.json'])
        self.assertEqual(trace_obj['otherData'], {})
        self.assertEqual(len(trace_obj['traceEvents']), 5)
        event = trace_obj['traceEvents'][0]
        self.assertEqual(event['name'], 'pass_test.PassingTest.test_pass')
        self.assertEqual(event['ph'], 'X')
        self.assertEqual(event['tid'], 1)
        self.assertEqual(event['args']['expected'], ['Pass'])
        self.assertEqual(event['args']['actual'], 'Pass')


class TestMain(TestCli):
    prog = []

    def make_host(self):
        return Host()

    def call(self, host, argv, stdin, env):
        stdin = unicode(stdin)
        host.stdin = io.StringIO(stdin)
        if env:
            host.getenv = env.get
        host.capture_output()
        orig_sys_path = sys.path[:]
        orig_sys_modules = list(sys.modules.keys())

        try:
            ret = main(argv + ['-j', '1'], host)
        finally:
            out, err = host.restore_output()
            modules_to_unload = []
            for k in sys.modules:
                if k not in orig_sys_modules:
                    modules_to_unload.append(k)
            for k in modules_to_unload:
                del sys.modules[k]
            sys.path = orig_sys_path

        return ret, out, err

    def test_debugger(self):
        # TODO: this test seems to hang under coverage.
        pass
