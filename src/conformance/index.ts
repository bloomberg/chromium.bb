import { Test } from 'tape';
import test from 'framework/mytape';

import './buffers';

test('true', (t: Test) => {
  t.true(true);
});
