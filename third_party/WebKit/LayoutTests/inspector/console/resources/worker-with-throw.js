function foo()
{
    throw new Error();
}
function boo()
{
    foo();
}

onmessage = function(event) {
    boo();
};