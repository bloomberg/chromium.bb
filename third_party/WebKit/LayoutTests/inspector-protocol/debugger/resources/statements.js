function statementsExample()
{
    var self = arguments.callee;

    debugger;

    self.step = 1;

    self.step = 2;

    void [
        self.step = 3,
        self.step = 4,
        self.step = 5,
        self.step = 6
    ];

    self.step = 7;
}