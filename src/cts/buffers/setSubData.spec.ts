export const description = `
setSubData tests.
`;

import { poptions, TestGroup } from "../../framework/index.js";
import { GPUTest } from "../gpu_test.js";

export const group = new TestGroup();

for (const params of poptions("value", [ 187, 110 ])) {
  group.testpf("basic", params, GPUTest, async (t) => {
    const value = t.params.value;

    const src = t.device.createBuffer({ size: 4, usage: 4 | 8 });
    src.setSubData(0, new Uint8Array([value]).buffer);
    await t.expectContents(src, new Uint8Array([value, 0, 0, 0]));
  });
}
