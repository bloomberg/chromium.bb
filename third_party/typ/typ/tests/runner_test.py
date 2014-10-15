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

from textwrap import dedent as d


from typ import Host, Runner, TestCase, TestSet, TestInput


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
        ret = r.main(foo='bar')
        self.assertEqual(ret, 2)

    def test_good_default(self):
        r = Runner()
        ret = r.main(tests=['typ.tests.runner_test.ContextTests'])
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


class ContextTests(TestCase):
    def test_context(self):
        # This test is mostly intended to be called by
        # RunnerTests.test_context, above. It is not interesting on its own.
        if self.context:
            self.assertEquals(self.context['foo'], 'bar')
