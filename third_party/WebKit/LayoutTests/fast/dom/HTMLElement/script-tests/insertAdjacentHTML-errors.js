description('Test insertAdjacentHTML exceptions to make sure they match HTML5');

var div = document.createElement("div");

shouldBeUndefined("div.insertAdjacentHTML('beforeBegin', 'text')");
shouldBeUndefined("div.insertAdjacentHTML('afterEnd', 'text')");

shouldThrow("div.insertAdjacentHTML('FOO', 'text')", '"SyntaxError: Failed to execute \'insertAdjacentHTML\' on \'HTMLElement\': The value provided (\'FOO\') is not one of \'beforeBegin\', \'afterBegin\', \'beforeEnd\', or \'afterEnd\'."');
shouldThrow("document.documentElement.insertAdjacentHTML('afterEnd', 'text')", '"NoModificationAllowedError: Failed to execute \'insertAdjacentHTML\' on \'HTMLElement\': The element has no parent."');
