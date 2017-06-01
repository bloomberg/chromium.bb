TestRunner.loadLazyModules(["quick_open"]).then(test);
function test() {
    TestRunner.addResult("Check to see that FilteredItemSelectionDialog uses proper regex to filter results.");

    var overridenInput = [];
    var history = [];

    var StubProvider = class extends QuickOpen.FilteredListWidget.Provider {
        itemKeyAt(itemIndex) { return overridenInput[itemIndex]; }
        itemScoreAt(itemIndex) { return 0; }
        itemCount() { return overridenInput.length; }
        selectItem(itemIndex, promptValue)
        {
            TestRunner.addResult("Selected item index: " + itemIndex);
        }
    };

    var provider = new StubProvider();

    function checkQuery(query, input)
    {
        overridenInput = input;

        TestRunner.addResult("Input:" + JSON.stringify(input));

        var filteredSelectionDialog = new QuickOpen.FilteredListWidget(provider, history);
        filteredSelectionDialog.showAsDialog();
        var promise = TestRunner.addSniffer(filteredSelectionDialog, "_itemsFilteredForTest").then(accept);
        filteredSelectionDialog.setQuery(query);
        filteredSelectionDialog._updateAfterItemsLoaded();
        return promise;

        function dump()
        {
            TestRunner.addResult("Query:" + JSON.stringify(filteredSelectionDialog._value()));
            var items = filteredSelectionDialog._items;
            var output = [];
            for (var i = 0; i < items.length(); ++i)
                output.push(provider.itemKeyAt(items.itemAtIndex(i)));
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

        function dumplicateSymbolsInQuery()
        {
            return checkQuery("aab", ["abab", "abaa", "caab", "baac", "fooaab"]);
        },

        function dangerousInputEscaping()
        {
            return checkQuery("^[]{}()\\.$*+?|", ["^[]{}()\\.$*+?|", "0123456789abcdef"]);
        },

        function itemIndexIsNotReportedInGoToLine()
        {
            return checkQuery(":1", []);
        },

        function autoCompleteIsLast()
        {
            return checkQuery("", ["abc", "abcd"]);
        }
    ]);
}
