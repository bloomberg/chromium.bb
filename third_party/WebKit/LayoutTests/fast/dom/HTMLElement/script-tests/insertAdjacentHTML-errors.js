description('Test insertAdjacentHTML exceptions to make sure they match HTML5');

var div = document.createElement("div");

shouldBeUndefined("div.insertAdjacentHTML('beforeBegin', 'text')");
shouldBeUndefined("div.insertAdjacentHTML('afterEnd', 'text')");

shouldThrow("div.insertAdjacentHTML('FOO', 'text')", '"SyntaxError: An invalid or illegal string was specified."');
shouldThrow("document.documentElement.insertAdjacentHTML('afterEnd', 'text')", '"NoModificationAllowedError: An attempt was made to modify an object where modifications are not allowed."');
