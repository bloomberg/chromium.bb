(async function() {
    TestRunner.runTests([
        function testEmptyPrefixSuffix() {
            const tokens = String.tokenizeFormatString(`%c%s`, {
                c: () => {},
                s: () => {}
            });
            TestRunner.addObject(tokens);
        },

        function testAnsiColors() {
            const types = [3, 9, 4, 10];
            const colors = [];
            for (const type of types) {
                for (let i = 0; i < 10; ++i)
                    colors.push(type * 10 + i)
            }
            const tokens = String.tokenizeFormatString(colors.map(c => `\u001b[${c}m`).join(''), {c: _ => {}});
            TestRunner.addArray(tokens);
        }
    ]);
})();
