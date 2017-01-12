TestRunner.loadLazyModules(["quick_open"]).then(test);
function test() {
    TestRunner.addResult("Check to see that FilteredItemSelectionDialog uses proper regex to filter results.");

    var overridenInput = [];
    var overrideShowMatchingItems = true;
    var history = [];

    var StubDelegate = class extends QuickOpen.FilteredListWidget.Delegate {
        itemKeyAt(itemIndex) { return overridenInput[itemIndex]; }
        itemScoreAt(itemIndex) { return 0; }
        itemCount() { return overridenInput.length; }
        selectItem(itemIndex, promptValue)
        {
            TestRunner.addResult("Selected item index: " + itemIndex);
        }
        shouldShowMatchingItems () { return overrideShowMatchingItems; }
    };

    var delegate = new StubDelegate();

    function checkQuery(query, input, hideMatchingItems)
    {
        overridenInput = input;
        overrideShowMatchingItems = !hideMatchingItems;

        TestRunner.addResult("Input:" + JSON.stringify(input));

        var filteredSelectionDialog = new QuickOpen.FilteredListWidget(delegate, history);
        filteredSelectionDialog.showAsDialog();
        var promise = TestRunner.addSniffer(filteredSelectionDialog, "_itemsFilteredForTest").then(accept);
        filteredSelectionDialog.setQuery(query);
        filteredSelectionDialog._updateAfterItemsLoaded();
        return promise;

        function dump()
        {
            TestRunner.addResult("Query:" + JSON.stringify(filteredSelectionDialog._value()));
            var list = filteredSelectionDialog._list;
            var output = [];
            for (var i = 0; i < list.length(); ++i)
                output.push(delegate.itemKeyAt(list.itemAtIndex(i)));
            TestRunner.addResult("Output:" + JSON.stringify(output));
        }

        function accept()
        {
            dump();
            filteredSelectionDialog._onEnter(TestRunner.createKeyEvent("Enter"));
            TestRunner.addResult("History:" + JSON.stringify(history));
        }
    }

    TestRunner.runTests([
        function emptyQueryMatchesEverything()
        {
            return checkQuery("", ["a", "bc"]);
        },

        function caseSensitiveMatching()
        {
            return checkQuery("aB", ["abc", "acB"]);
        },

        function caseInsensitiveMatching()
        {
            return checkQuery("ab", ["abc", "bac", "a_B"]);
        },

        function dumplicateSymbolsInQuery(next)
        {
            return checkQuery("aab", ["abab", "abaa", "caab", "baac", "fooaab"]);
        },

        function dangerousInputEscaping(next)
        {
            return checkQuery("^[]{}()\\.$*+?|", ["^[]{}()\\.$*+?|", "0123456789abcdef"]);
        },

        function itemIndexIsNotReportedInGoToLine(next)
        {
            return checkQuery(":1", [":1:2:3.js"], true, next);
        },

        function autoCompleteIsLast(next)
        {
            return checkQuery("", ["abc", "abcd"]);
        }
    ]);
}
