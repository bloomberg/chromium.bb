function notBlackboxedFoo()
{
    var a = 42;
    var b = blackboxedBoo();
    return a + b;
}

function blackboxedFoo()
{
    var a = 42;
    var b = notBlackboxedFoo();
    return a + b;
}

function notBlackboxedBoo()
{
    var a = 42;
    var b = blackboxedFoo();
    return a + b;
}
//# sourceURL=mixed-source.js
