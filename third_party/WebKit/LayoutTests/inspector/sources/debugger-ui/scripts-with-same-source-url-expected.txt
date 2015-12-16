Tests that script is replaced with the newer version when the names match.

Added: MyScript.js to network
Added: debugger:///VMXX MyScript.js to debugger
Removed: MyScript.js from network
Added: MyScript.js to network
Added: debugger:///VMXX MyScript.js to debugger
Content: function foo() { return 1; } //# sourceURL=MyScript.js
Content: function foo() { return 2; } //# sourceURL=MyScript.js

