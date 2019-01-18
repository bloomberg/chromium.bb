export const description = `
setSubData tests.
`;

import { getGPU } from "../../framework/gpu/implementation.js";
import { TestGroup } from "../../framework/index.js";

export const group = new TestGroup();

group.test("basic", async (t) => {
  const gpu = await getGPU();
  const d = gpu.getDevice();
  const q = d.getQueue();

  // TODO: everything before this point should be factored out into a setup helper.

  const size = 4;
  const testvalue = 187;
  const src = d.createBuffer({
    size,
    usage: 4 | 8,
  });
  src.setSubData(0, new Uint8Array([testvalue]).buffer);

  // TODO: everything after this point should be factored out into a check-buffer-contents helper.

  const dst = d.createBuffer({
    size,
    usage: 1 | 8,
  });

  const c = d.createCommandBuffer({});
  c.copyBufferToBuffer(src, 0, dst, 0, size);

  q.submit([c]);

  let done = false;
  dst.mapReadAsync(0, size, (ab: ArrayBuffer) => {
    const data = new Uint8Array(ab);
    t.expect(data[0] === testvalue);
    t.expect(data[1] === 0);
    t.expect(data[2] === 0);
    t.expect(data[3] === 0);
    done = true;
  });

  t.log("waiting...");

  while (!done) {
    d.flush();
    await wait();
  }
  t.log("done!");
});

function wait() {
  return new Promise((resolve) => { setTimeout(resolve, 10); });
}
