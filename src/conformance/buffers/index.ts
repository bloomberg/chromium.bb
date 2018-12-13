import { Test } from 'tape';
import test from 'framework/mytape';

import './foo';

test('true', (t: Test) => {
  t.true(true);
});
