import { test, Test } from 'framework/mytape';

test('fail', (t: Test) => {
  t.true(false);
});

test('sync', (t: Test) => {
  t.true(true);
});

test('async', async (t: Test) => {
  await new Promise(res => {
    setTimeout(res, 100);
  })
  t.true(true);
});
