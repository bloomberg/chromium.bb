export const description = `
Basic command buffer compute tests.
`;

import { TestGroup } from "../../../framework/index.js";
import { GPUTest } from "../../gpu_test.js";

export const group = new TestGroup();

group.testf("memcpy", GPUTest, async (t) => {
  const data = new Uint32Array([0x01020304]).buffer;
  const src = t.device.createBuffer({ size: 4, usage: 4 | 8 });
  const dst = t.device.createBuffer({ size: 4, usage: 4 | 8 });
  src.setSubData(0, data);

  const module = t.device.createShaderModule({
    code: new Uint8Array([
      // TODO: put a shader here (maybe just use shaderc.js for now)
    ]).buffer,
  });
  const pipeline = t.device.createComputePipeline({
    computeStage: { module }
  });

  const encoder = t.device.createCommandEncoder({})
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.dispatch(1);
  pass.endPass();
  t.device.getQueue().submit([encoder.finish()]);

  await t.expectContents(dst, new Uint8Array(data));
});
