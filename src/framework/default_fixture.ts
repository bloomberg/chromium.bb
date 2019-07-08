import { Fixture } from './fixture.js';

export class DefaultFixture extends Fixture {
  private outstanding = 0;

  finalize(): void {
    if (this.outstanding !== 0) {
      throw new Error(
        'there were outstanding asynchronous expectations (e.g. shouldReject) at the end of the test'
      );
    }
  }

  warn(msg?: string): void {
    this.rec.warn(msg);
  }

  fail(msg?: string): void {
    this.rec.fail(msg);
  }

  ok(msg?: string): void {
    const m = msg ? ': ' + msg : '';
    this.log('OK' + m);
  }

  async shouldReject(p: Promise<unknown>, msg?: string): Promise<void> {
    this.outstanding++;
    const m = msg ? ': ' + msg : '';
    try {
      await p;
      this.fail('DID NOT THROW' + m);
    } catch (ex) {
      this.ok('threw' + m);
    }
    this.outstanding--;
  }

  shouldThrow(fn: () => void, msg?: string): void {
    const m = msg ? ': ' + msg : '';
    try {
      fn();
      this.fail('DID NOT THROW' + m);
    } catch (ex) {
      this.ok('threw' + m);
    }
  }

  expect(cond: boolean, msg?: string): boolean {
    if (cond) {
      this.ok(msg);
    } else {
      this.rec.fail(msg);
    }
    return cond;
  }
}
