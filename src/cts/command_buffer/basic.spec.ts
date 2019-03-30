export const description = `
Basic tests.
`;

import { TestGroup } from "../../framework/index.js";
import { GPUTest } from "../gpu_test.js";

export const group = new TestGroup();

group.test("empty", GPUTest, async (t) => {
  const encoder = t.device.createCommandEncoder({})
  const cmd = encoder.finish();
  t.device.getQueue().submit([cmd]);

  // TODO: test that submit() succeeded.
});
