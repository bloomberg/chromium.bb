import { Fixture } from './test_group.js';

export class DefaultFixture extends Fixture {
  public warn(msg?: string) {
    this.rec.warn(msg);
  }

  public fail(msg?: string) {
    this.rec.fail(msg);
  }

  public ok(msg?: string) {
    if (msg) {
      this.log('OK: ' + msg);
    } else {
      this.log('OK');
    }
  }

  public expect(cond: boolean, msg?: string) {
    if (cond) {
      this.ok(msg);
    } else {
      this.rec.fail(msg);
    }
  }
}
