export const description = ``;

import { Fixture, TestGroup } from '../../../framework/index.js';

export const g = new TestGroup(Fixture);

g.test('canvas element getContext returns GPUCanvasContext', async t => {
  if (typeof document === 'undefined') {
    // Skip if there is no document (Workers, Node)
    t.skip('DOM is not available to create canvas element');
  }

  const canvas = document.createElement('canvas');
  canvas.width = 10;
  canvas.height = 10;

  // TODO: fix types so these aren't necessary
  // tslint:disable-next-line: no-any
  const ctx: any = canvas.getContext('gpupresent');
  // tslint:disable-next-line: no-any
  t.expect(ctx instanceof (window as any).GPUCanvasContext);
});
