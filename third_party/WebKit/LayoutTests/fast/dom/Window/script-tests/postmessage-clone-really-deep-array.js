document.getElementById("description").innerHTML = "Tests that we abort cloning overdeep arrays.";
reallyDeepArray=[];
for (var i = 0; i < 100000; i++)
    reallyDeepArray=[reallyDeepArray];
tryPostMessage('reallyDeepArray', true, null, RangeError);
tryPostMessage('"done"');
