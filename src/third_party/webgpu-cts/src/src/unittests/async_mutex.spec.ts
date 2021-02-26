export const description = `
Tests for AsyncMutex.
`;

import { makeTestGroup } from '../common/framework/test_group.js';
import { AsyncMutex } from '../common/framework/util/async_mutex.js';
import { objectEquals } from '../common/framework/util/util.js';

import { UnitTest } from './unit_test.js';

export const g = makeTestGroup(UnitTest);

/* eslint-disable-next-line  @typescript-eslint/no-unused-vars */
g.test('basic').fn(async t => {
  const mutex = new AsyncMutex();
  await mutex.with(async () => {});
});

/* eslint-disable-next-line  @typescript-eslint/no-unused-vars */
g.test('serial').fn(async t => {
  const mutex = new AsyncMutex();
  await mutex.with(async () => {});
  await mutex.with(async () => {});
  await mutex.with(async () => {});
});

/* eslint-disable-next-line  @typescript-eslint/no-unused-vars */
g.test('parallel').fn(async t => {
  const mutex = new AsyncMutex();
  await Promise.all([
    mutex.with(async () => {}),
    mutex.with(async () => {}),
    mutex.with(async () => {}),
  ]);
});

g.test('parallel,many').fn(async t => {
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

g.test('return').fn(async t => {
  const mutex = new AsyncMutex();
  const ret = await mutex.with(async () => 123);
  t.expect(ret === 123);
});

g.test('return,parallel').fn(async t => {
  const mutex = new AsyncMutex();
  const ret = await Promise.all([
    mutex.with(async () => 1),
    mutex.with(async () => 2),
    mutex.with(async () => 3),
  ]);
  t.expect(objectEquals(ret, [1, 2, 3]));
});
