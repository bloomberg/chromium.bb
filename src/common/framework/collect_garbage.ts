// tslint:disable-next-line: no-any
declare const Components: any;

export function attemptGarbageCollection(): void {
  // tslint:disable-next-line: no-any
  const w: any = self;
  if (w.GCController) {
    w.GCController.collect();
    return;
  }

  if (w.opera && w.opera.collect) {
    w.opera.collect();
    return;
  }

  try {
    w.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
      .getInterface(Components.interfaces.nsIDOMWindowUtils)
      .garbageCollect();
    return;
  } catch (e) {}

  if (w.gc) {
    w.gc();
    return;
  }

  if (w.CollectGarbage) {
    w.CollectGarbage();
    return;
  }

  let i: number;
  function gcRec(n: number): void {
    if (n < 1) return;
    // tslint:disable-next-line: no-any
    let temp: any = { i: 'ab' + i + i / 100000 };
    temp = temp + 'foo';
    gcRec(n - 1);
  }
  for (i = 0; i < 1000; i++) {
    gcRec(10);
  }
}
