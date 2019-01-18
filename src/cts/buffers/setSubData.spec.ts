export const description = `
setSubData tests.
`;

import { TestGroup } from "../../framework/index.js";
import { GPUTest } from "../gpu_test.js";

export const group = new TestGroup();

group.testf("basic", GPUTest, async (t) => {
  const src = t.device.createBuffer({ size: 4, usage: 4 | 8 });
  src.setSubData(0, new Uint8Array([187]).buffer);
  await t.expectContents(src, new Uint8Array([187, 0, 0, 0]));
});
