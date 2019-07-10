import { TestCaseID } from '../../framework/id.js';
import { Fixture, TestGroup, paramsEquals } from '../../framework/index.js';
import { LiveTestSpecResult, Logger } from '../../framework/logger.js';

import { UnitTest } from './unit_test.js';

export class TestGroupTest extends UnitTest {
  async run<F extends Fixture>(g: TestGroup<F>): Promise<LiveTestSpecResult> {
    const [rec, res] = new Logger().record({ suite: '', path: '' });
    await Promise.all(Array.from(g.iterate(rec)).map(test => test.run()));
    return res;
  }

  enumerate<F extends Fixture>(g: TestGroup<F>): TestCaseID[] {
    const cases = [];
    const [rec] = new Logger().record({ suite: '', path: '' });
    for (const test of g.iterate(rec)) {
      cases.push(test.id);
    }
    return cases;
  }

  expectCases<F extends Fixture>(g: TestGroup<F>, cases: TestCaseID[]): void {
    const gcases = this.enumerate(g);

    if (this.expect(gcases.length === cases.length)) {
      for (let i = 0; i < cases.length; ++i) {
        this.expect(gcases[i].test === cases[i].test);
        this.expect(paramsEquals(gcases[i].params, cases[i].params));
      }
    }
  }
}
