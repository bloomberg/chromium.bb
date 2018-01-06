// Test helpers used by multiple Web Locks API tests.
(() => {

  let res_num = 0;
  self.uniqueName = testCase => {
    return `${self.location.pathname}-${testCase.name}-${++res_num}`;
  };

})();
