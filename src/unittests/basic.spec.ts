export const description = `
Basic unit tests for test framework.
`;

import { DefaultFixture } from '../framework/default_fixture.js';
import {
  TestGroup,
} from '../framework/index.js';

export const group = new TestGroup();

group.test('test_sync', DefaultFixture, (t) => {
});

group.test('test_async', DefaultFixture, async (t) => {
});

group.testp('testp_sync', {}, DefaultFixture, (t) => {
  t.log(JSON.stringify(t.params));
});

group.testp('testp_async', {}, DefaultFixture, async (t) => {
  t.log(JSON.stringify(t.params));
});
