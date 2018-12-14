import tape from 'tape';

export type Test = tape.Test;

export function test(name: string, cb: tape.TestCase) {
  tape(name, async (t: tape.Test) => {
    await cb(t);
    t.end();
  });
}

// TODO: test.skip
// TODO: take params
// TODO: handle exceptions?
