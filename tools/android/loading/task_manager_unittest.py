# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import re
import shutil
import StringIO
import sys
import tempfile
import unittest

import task_manager


_GOLDEN_GRAPHVIZ = """digraph graphname {
  n0 [label="0: b", color=black, shape=ellipse];
  n1 [label="1: c", color=black, shape=ellipse];
  n0 -> n1;
  n2 [label="a", color=blue, shape=plaintext];
  n2 -> n1;
  n3 [label="2: d", color=black, shape=ellipse];
  n1 -> n3;
  n4 [label="3: f", color=black, shape=box];
  n3 -> n4;
  n5 [label="e", color=blue, shape=ellipse];
  n5 -> n4;
}\n"""


@contextlib.contextmanager
def EatStdoutAndStderr():
  """Overrides sys.std{out,err} to intercept write calls."""
  sys.stdout.flush()
  sys.stderr.flush()
  original_stdout = sys.stdout
  original_stderr = sys.stderr
  try:
    sys.stdout = StringIO.StringIO()
    sys.stderr = StringIO.StringIO()
    yield
  finally:
    sys.stdout = original_stdout
    sys.stderr = original_stderr


class TestException(Exception):
  pass


class TaskManagerTestCase(unittest.TestCase):
  def setUp(self):
    self.output_directory = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.output_directory)

  def TouchOutputFile(self, file_path):
    with open(os.path.join(self.output_directory, file_path), 'w') as output:
      output.write(file_path + '\n')


class TaskTest(TaskManagerTestCase):
  def testStaticTask(self):
    task = task_manager.Task('hello.json', 'what/ever/hello.json', [], None)
    self.assertTrue(task.IsStatic())
    self.assertTrue(task._is_done)
    with self.assertRaises(task_manager.TaskError):
      task.Execute()

  def testDynamicTask(self):
    def Recipe():
      Recipe.counter += 1
    Recipe.counter = 0
    task = task_manager.Task('hello.json', 'what/ever/hello.json', [], Recipe)
    self.assertFalse(task.IsStatic())
    self.assertFalse(task._is_done)
    self.assertEqual(0, Recipe.counter)
    task.Execute()
    self.assertEqual(1, Recipe.counter)
    task.Execute()
    self.assertEqual(1, Recipe.counter)

  def testDynamicTaskWithUnexecutedDeps(self):
    def RecipeA():
      self.fail()

    def RecipeB():
      RecipeB.counter += 1
    RecipeB.counter = 0

    a = task_manager.Task('hello.json', 'out/hello.json', [], RecipeA)
    b = task_manager.Task('hello.json', 'out/hello.json', [a], RecipeB)
    self.assertEqual(0, RecipeB.counter)
    b.Execute()
    self.assertEqual(1, RecipeB.counter)


class BuilderTest(TaskManagerTestCase):
  def testCreateUnexistingStaticTask(self):
    builder = task_manager.Builder(self.output_directory)
    with self.assertRaises(task_manager.TaskError):
      builder.CreateStaticTask('hello.txt', '/__unexisting/file/path')

  def testCreateStaticTask(self):
    builder = task_manager.Builder(self.output_directory)
    task = builder.CreateStaticTask('hello.py', __file__)
    self.assertTrue(task.IsStatic())

  def testDuplicateStaticTask(self):
    builder = task_manager.Builder(self.output_directory)
    builder.CreateStaticTask('hello.py', __file__)
    with self.assertRaises(task_manager.TaskError):
      builder.CreateStaticTask('hello.py', __file__)

  def testRegisterTask(self):
    builder = task_manager.Builder(self.output_directory)
    @builder.RegisterTask('hello.txt')
    def TaskA():
      TaskA.executed = True
    TaskA.executed = False
    self.assertFalse(TaskA.IsStatic())
    self.assertEqual(os.path.join(self.output_directory, 'hello.txt'),
                     TaskA.path)
    self.assertFalse(TaskA.executed)
    TaskA.Execute()
    self.assertTrue(TaskA.executed)

  def testRegisterDuplicateTask(self):
    builder = task_manager.Builder(self.output_directory)
    @builder.RegisterTask('hello.txt')
    def TaskA():
      pass
    del TaskA # unused
    with self.assertRaises(task_manager.TaskError):
      @builder.RegisterTask('hello.txt')
      def TaskB():
        pass
      del TaskB # unused

  def testTaskMerging(self):
    builder = task_manager.Builder(self.output_directory)
    @builder.RegisterTask('hello.txt')
    def TaskA():
      pass
    @builder.RegisterTask('hello.txt', merge=True)
    def TaskB():
      pass
    self.assertEqual(TaskA, TaskB)

  def testStaticTaskMergingError(self):
    builder = task_manager.Builder(self.output_directory)
    builder.CreateStaticTask('hello.py', __file__)
    with self.assertRaises(task_manager.TaskError):
      @builder.RegisterTask('hello.py', merge=True)
      def TaskA():
        pass
      del TaskA # unused


class GenerateScenarioTest(TaskManagerTestCase):
  def testParents(self):
    builder = task_manager.Builder(self.output_directory)
    @builder.RegisterTask('a')
    def TaskA():
      pass
    @builder.RegisterTask('b', dependencies=[TaskA])
    def TaskB():
      pass
    @builder.RegisterTask('c', dependencies=[TaskB])
    def TaskC():
      pass
    scenario = task_manager.GenerateScenario([TaskA, TaskB, TaskC], set())
    self.assertListEqual([TaskA, TaskB, TaskC], scenario)

    scenario = task_manager.GenerateScenario([TaskB], set())
    self.assertListEqual([TaskA, TaskB], scenario)

    scenario = task_manager.GenerateScenario([TaskC], set())
    self.assertListEqual([TaskA, TaskB, TaskC], scenario)

    scenario = task_manager.GenerateScenario([TaskC, TaskB], set())
    self.assertListEqual([TaskA, TaskB, TaskC], scenario)

  def testFreezing(self):
    builder = task_manager.Builder(self.output_directory)
    @builder.RegisterTask('a')
    def TaskA():
      pass
    @builder.RegisterTask('b', dependencies=[TaskA])
    def TaskB():
      pass
    @builder.RegisterTask('c')
    def TaskC():
      pass
    @builder.RegisterTask('d', dependencies=[TaskB, TaskC])
    def TaskD():
      pass

    # assert no exception raised.
    task_manager.GenerateScenario([TaskB], set([TaskC]))

    with self.assertRaises(task_manager.TaskError):
      task_manager.GenerateScenario([TaskD], set([TaskA]))

    self.TouchOutputFile('a')
    scenario = task_manager.GenerateScenario([TaskD], set([TaskA]))
    self.assertListEqual([TaskB, TaskC, TaskD], scenario)

    self.TouchOutputFile('b')
    scenario = task_manager.GenerateScenario([TaskD], set([TaskB]))
    self.assertListEqual([TaskC, TaskD], scenario)

  def testCycleError(self):
    builder = task_manager.Builder(self.output_directory)
    @builder.RegisterTask('a')
    def TaskA():
      pass
    @builder.RegisterTask('b', dependencies=[TaskA])
    def TaskB():
      pass
    @builder.RegisterTask('c', dependencies=[TaskB])
    def TaskC():
      pass
    @builder.RegisterTask('d', dependencies=[TaskC])
    def TaskD():
      pass
    TaskA._dependencies.append(TaskC)
    with self.assertRaises(task_manager.TaskError):
      task_manager.GenerateScenario([TaskD], set())

  def testCollisionError(self):
    builder_a = task_manager.Builder(self.output_directory)
    builder_b = task_manager.Builder(self.output_directory)
    @builder_a.RegisterTask('a')
    def TaskA():
      pass
    @builder_b.RegisterTask('a')
    def TaskB():
      pass
    with self.assertRaises(task_manager.TaskError):
      task_manager.GenerateScenario([TaskA, TaskB], set())

  def testGraphVizOutput(self):
    builder = task_manager.Builder(self.output_directory)
    static_task = builder.CreateStaticTask('a', __file__)
    @builder.RegisterTask('b')
    def TaskB():
      pass
    @builder.RegisterTask('c', dependencies=[TaskB, static_task])
    def TaskC():
      pass
    @builder.RegisterTask('d', dependencies=[TaskC])
    def TaskD():
      pass
    @builder.RegisterTask('e')
    def TaskE():
      pass
    @builder.RegisterTask('f', dependencies=[TaskD, TaskE])
    def TaskF():
      pass
    self.TouchOutputFile('e')
    scenario = task_manager.GenerateScenario([TaskF], set([TaskE]))
    output = StringIO.StringIO()
    task_manager.OutputGraphViz(scenario, [TaskF], output)
    self.assertEqual(_GOLDEN_GRAPHVIZ, output.getvalue())

  def testListResumingTasksToFreeze(self):
    TaskManagerTestCase.setUp(self)
    builder = task_manager.Builder(self.output_directory)
    static_task = builder.CreateStaticTask('static', __file__)
    @builder.RegisterTask('a')
    def TaskA():
      pass
    @builder.RegisterTask('b', dependencies=[static_task])
    def TaskB():
      pass
    @builder.RegisterTask('c', dependencies=[TaskA, TaskB])
    def TaskC():
      pass
    @builder.RegisterTask('d', dependencies=[TaskA])
    def TaskD():
      pass
    @builder.RegisterTask('e', dependencies=[TaskC])
    def TaskE():
      pass
    @builder.RegisterTask('f', dependencies=[TaskC])
    def TaskF():
      pass

    for k in 'abcdef':
      self.TouchOutputFile(k)

    def RunSubTest(final_tasks, initial_frozen_tasks, failed_task, reference):
      scenario = \
          task_manager.GenerateScenario(final_tasks, initial_frozen_tasks)
      resume_frozen_tasks = task_manager.ListResumingTasksToFreeze(
          scenario, final_tasks, failed_task)
      self.assertEqual(reference, resume_frozen_tasks)

      failed_pos = scenario.index(failed_task)
      new_scenario = \
          task_manager.GenerateScenario(final_tasks, resume_frozen_tasks)
      self.assertEqual(scenario[failed_pos:], new_scenario)

    RunSubTest([TaskA], set([]), TaskA, set([]))
    RunSubTest([TaskD], set([]), TaskA, set([]))
    RunSubTest([TaskD], set([]), TaskD, set([TaskA]))
    RunSubTest([TaskE, TaskF], set([TaskA]), TaskB, set([TaskA]))
    RunSubTest([TaskE, TaskF], set([TaskA]), TaskC, set([TaskA, TaskB]))
    RunSubTest([TaskE, TaskF], set([TaskA]), TaskE, set([TaskC]))
    RunSubTest([TaskE, TaskF], set([TaskA]), TaskF, set([TaskC, TaskE]))


class CommandLineControlledExecutionTest(TaskManagerTestCase):
  def Execute(self, *command_line_args):
    builder = task_manager.Builder(self.output_directory)
    @builder.RegisterTask('a')
    def TaskA():
      pass
    @builder.RegisterTask('b')
    def TaskB():
      pass
    @builder.RegisterTask('c', dependencies=[TaskA, TaskB])
    def TaskC():
      pass
    @builder.RegisterTask('d', dependencies=[TaskA])
    def TaskD():
      pass
    @builder.RegisterTask('e', dependencies=[TaskC])
    def TaskE():
      pass
    @builder.RegisterTask('raise_exception', dependencies=[TaskB])
    def TaskF():
      raise TestException('Expected error.')

    default_final_tasks = [TaskD, TaskE]
    parser = task_manager.CommandLineParser()
    cmd = ['-o', self.output_directory]
    cmd.extend([i for i in command_line_args])
    args = parser.parse_args(cmd)
    with EatStdoutAndStderr():
      return task_manager.ExecuteWithCommandLine(
          args, [TaskA, TaskB, TaskC, TaskD, TaskE, TaskF], default_final_tasks)

  def testSimple(self):
    self.assertEqual(0, self.Execute())

  def testDryRun(self):
    self.assertEqual(0, self.Execute('-d'))

  def testRegex(self):
    self.assertEqual(0, self.Execute('-e', 'b', 'd'))
    self.assertEqual(1, self.Execute('-e', r'\d'))

  def testFreezing(self):
    self.assertEqual(0, self.Execute('-f', r'\d'))
    self.TouchOutputFile('c')
    self.assertEqual(0, self.Execute('-f', 'c'))

  def testTaskFailure(self):
    with self.assertRaisesRegexp(TestException, r'^Expected error\.$'):
      self.Execute('-e', 'raise_exception')


if __name__ == '__main__':
  unittest.main()
