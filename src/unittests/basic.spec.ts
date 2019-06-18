export const description = `
Basic unit tests for test framework.
`;

import { DefaultFixture } from '../framework/default_fixture.js';
import {
  TestGroup,
} from '../framework/index.js';

export const group = new TestGroup();

group.test('test/sync', DefaultFixture, (t) => { });

group.test('test/async', DefaultFixture, async (t) => { });

group.test('testp/sync', DefaultFixture, (t) => {
  t.log(JSON.stringify(t.params));
}, {});

group.test('testp/async', DefaultFixture, async (t) => {
  t.log(JSON.stringify(t.params));
}, {});
