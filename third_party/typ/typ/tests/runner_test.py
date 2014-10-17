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

import tempfile

from textwrap import dedent as d


from typ import Host, Runner, TestCase, TestSet, TestInput
from typ import WinMultiprocessing, main


def _setup_process(child, context):  # pylint: disable=W0613
    return context


def _teardown_process(child, context):  # pylint: disable=W0613
    return context


class RunnerTests(TestCase):
    def test_context(self):
        r = Runner()
        r.args.tests = ['typ.tests.runner_test.ContextTests']
        ret, _, _ = r.run(context={'foo': 'bar'}, setup_fn=_setup_process,
                          teardown_fn=_teardown_process)
        self.assertEqual(ret, 0)

    def test_bad_default(self):
        r = Runner()
        ret = r.main([], foo='bar')
        self.assertEqual(ret, 2)

    def test_good_default(self):
        r = Runner()
        ret = r.main([], tests=['typ.tests.runner_test.ContextTests'])
        self.assertEqual(ret, 0)


class TestSetTests(TestCase):
    # This class exists to test the failures that can come up if you
    # create your own test sets and bypass find_tests(); failures that
    # would normally be caught there can occur later during test execution.

    def test_missing_name(self):
        test_set = TestSet()
        test_set.parallel_tests = [TestInput('nonexistent test')]
        r = Runner()
        ret, _, _ = r.run(test_set)
        self.assertEqual(ret, 1)

    def test_failing_load_test(self):
        h = Host()
        orig_wd = h.getcwd()
        tmpdir = None
        try:
            tmpdir = h.mkdtemp()
            h.chdir(tmpdir)
            h.write_text_file('load_test.py', d("""\
                import unittest
                def load_tests(_, _2, _3):
                    assert False
                """))
            test_set = TestSet()
            test_set.parallel_tests = [TestInput('load_test.BaseTest.test_x')]
            r = Runner()
            ret, _, _ = r.run(test_set)
            self.assertEqual(ret, 1)
        finally:
            h.chdir(orig_wd)
            if tmpdir:
                h.rmtree(tmpdir)


class TestWinMultiprocessing(TestCase):
    def make_host(self):
        return Host()

    def call(self, argv, platform=None, importable=None, **kwargs):
        h = self.make_host()
        orig_wd = h.getcwd()
        tmpdir = None
        try:
            tmpdir = h.mkdtemp()
            h.chdir(tmpdir)
            h.capture_output()
            if platform is not None:
                h.platform = platform
            r = Runner(h)
            if importable is not None:
                r._main_is_importable = lambda: importable
            ret = r.main(argv, **kwargs)
        finally:
            out, err = h.restore_output()
            h.chdir(orig_wd)
            if tmpdir:
                h.rmtree(tmpdir)

        return ret, out, err

    def test_bad_value(self):
        self.assertRaises(ValueError, self.call, [], win_multiprocessing='foo')

    def test_force(self):
        h = self.make_host()
        tmpdir = None
        orig_wd = h.getcwd()
        out = err = None
        out_str = err_str = ''
        try:
            tmpdir = h.mkdtemp()
            h.chdir(tmpdir)
            out = tempfile.NamedTemporaryFile(delete=False)
            err = tempfile.NamedTemporaryFile(delete=False)
            ret = main([], stdout=out, stderr=err,
                       win_multiprocessing=WinMultiprocessing.force)
        finally:
            h.chdir(orig_wd)
            if tmpdir:
                h.rmtree(tmpdir)
            if out:
                out.close()
                out = open(out.name)
                out_str = out.read()
                out.close()
                h.remove(out.name)
            if err:
                err.close()
                err = open(err.name)
                err_str = err.read()
                err.close()
                h.remove(err.name)

        self.assertEqual(ret, 1)
        self.assertEqual(out_str, 'No tests to run.\n')
        self.assertEqual(err_str, '')

    def test_ignore(self):
        h = self.make_host()
        if h.platform == 'win32':  # pragma: win32
            self.assertRaises(ValueError, self.call, [],
                              win_multiprocessing=WinMultiprocessing.ignore)
        else:
            result = self.call([], platform=None, importable=False,
                               win_multiprocessing=WinMultiprocessing.ignore)
            ret, out, err = result
            self.assertEqual(ret, 1)
            self.assertEqual(out, 'No tests to run.\n')
            self.assertEqual(err, '')

    def test_multiple_jobs(self):
        self.assertRaises(ValueError, self.call, ['-j', '2'],
                          platform='win32', importable=False)

    def test_normal(self):
        # This tests that typ itself is importable ...
        ret, out, err = self.call([])
        self.assertEqual(ret, 1)
        self.assertEqual(out, 'No tests to run.\n')
        self.assertEqual(err, '')

    def test_real_unimportable_main(self):
        h = self.make_host()
        tmpdir = None
        orig_wd = h.getcwd()
        out = err = None
        out_str = err_str = ''
        try:
            tmpdir = h.mkdtemp()
            h.chdir(tmpdir)
            out = tempfile.NamedTemporaryFile(delete=False)
            err = tempfile.NamedTemporaryFile(delete=False)
            path_above_typ = h.realpath(h.dirname(__file__), '..', '..')
            env = {'PYTHONPATH': path_above_typ}
            h.write_text_file('test', d("""
                import sys
                import typ
                sys.exit(typ.main())
                """))
            h.stdout = out
            h.stderr = err
            ret = h.call_inline([h.python_interpreter, h.join(tmpdir, 'test')],
                                env=env)
        finally:
            h.chdir(orig_wd)
            if tmpdir:
                h.rmtree(tmpdir)
            if out:
                out.close()
                out = open(out.name)
                out_str = out.read()
                out.close()
                h.remove(out.name)
            if err:
                err.close()
                err = open(err.name)
                err_str = err.read()
                err.close()
                h.remove(err.name)

        self.assertEqual(ret, 1)
        self.assertEqual(out_str, '')
        self.assertIn('ValueError: The __main__ module is not importable',
                      err_str)

    def test_run_serially(self):
        ret, out, err = self.call([], importable=False,
                                  win_multiprocessing=WinMultiprocessing.spawn)
        self.assertEqual(ret, 1)
        self.assertEqual(out, 'No tests to run.\n')
        self.assertEqual(err, '')

    def test_single_job(self):
        ret, out, err = self.call(['-j', '1'], platform='win32',
                                  importable=False)
        self.assertEqual(ret, 1)
        self.assertEqual(out, 'No tests to run.\n')
        self.assertEqual(err, '')

    def test_spawn(self):
        ret, out, err = self.call([], importable=False,
                                  win_multiprocessing=WinMultiprocessing.spawn)
        self.assertEqual(ret, 1)
        self.assertEqual(out, 'No tests to run.\n')
        self.assertEqual(err, '')


class ContextTests(TestCase):
    def test_context(self):
        # This test is mostly intended to be called by
        # RunnerTests.test_context, above. It is not interesting on its own.
        if self.context:
            self.assertEquals(self.context['foo'], 'bar')
