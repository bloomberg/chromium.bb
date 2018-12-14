import { test, Test } from 'framework/mytape';
import './foo';

test('true', (t: Test) => {
  t.true(true);
});
