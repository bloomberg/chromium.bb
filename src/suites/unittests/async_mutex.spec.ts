export const description = `
Tests for AsyncMutex.
`;

import { TestGroup, objectEquals } from '../../framework/index.js';
import { AsyncMutex } from '../../framework/util/async_mutex.js';

import { UnitTest } from './unit_test.js';

export const g = new TestGroup(UnitTest);

g.test('basic', async t => {
  const mutex = new AsyncMutex();
  await mutex.with(async () => {});
});

g.test('serial', async t => {
  const mutex = new AsyncMutex();
  await mutex.with(async () => {});
  await mutex.with(async () => {});
  await mutex.with(async () => {});
});

g.test('parallel', async t => {
  const mutex = new AsyncMutex();
  await Promise.all([
    mutex.with(async () => {}),
    mutex.with(async () => {}),
    mutex.with(async () => {}),
  ]);
});

g.test('parallel/many', async t => {
  const mutex = new AsyncMutex();
  const actual: number[] = [];
  const expected = [];
  for (let i = 0; i < 1000; ++i) {
    expected.push(i);
    mutex.with(async () => {
      actual.push(i);
    });
  }
  await mutex.with(async () => {});
  t.expect(objectEquals(actual, expected));
});

g.test('return', async t => {
  const mutex = new AsyncMutex();
  const ret = await mutex.with(async () => 123);
  t.expect(ret === 123);
});

g.test('return/parallel', async t => {
  const mutex = new AsyncMutex();
  const ret = await Promise.all([
    mutex.with(async () => 1),
    mutex.with(async () => 2),
    mutex.with(async () => 3),
  ]);
  t.expect(objectEquals(ret, [1, 2, 3]));
});
