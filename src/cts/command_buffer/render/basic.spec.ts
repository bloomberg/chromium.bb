export const description = `
Basic command buffer rendering tests.
`;

import { TestGroup } from "../../../framework/index.js";
import { GPUTest } from "../../gpu_test.js";

export const group = new TestGroup();

group.testf("clear", GPUTest, async (t) => {
  const dst = t.device.createBuffer({ size: 4, usage: 4 | 8 });

  const colorAttachment = t.device.createTexture({
    size: { width: 1, height: 1, depth: 1 },
    format: "rgba8unorm",
    usage: 1 | 16,
  });
  const colorAttachmentView = colorAttachment.createDefaultView();

  const encoder = t.device.createCommandEncoder({})
  const pass = encoder.beginRenderPass({
    colorAttachments: [
      {
        attachment: colorAttachmentView,
        loadOp: "clear",
        clearColor: { r: 0.0, g: 1.0, b: 0.0, a: 1.0 }
      }
    ],
  });
  pass.endPass();
  encoder.copyTextureToBuffer(
    { texture: colorAttachment, mipLevel: 0, origin: { x: 0, y: 0, z: 0} },
    { buffer: dst, rowPitch: 256, imageHeight: 1 },
    { width: 1, height: 1, depth: 1});
  t.device.getQueue().submit([encoder.finish()]);

  await t.expectContents(dst, new Uint8Array([0x00, 0xff, 0x00, 0xff]));
});
