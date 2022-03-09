// Test that functions within modules work

// This should be shadowed by the module's function
function Bool inv(Bit#(4) x) = x[0] == 0? True: False;

module Foo;
    Reg#(Bool) r(False);
    function Bool inv(Bool x) = !x;
    // Parametric functions within modules are currently disallowed
    //function Bool test#(Integer n)(Bool x) = x;
    method Bool m = inv(r);
    rule tick;
        r <= inv(r);
    endrule
endmodule

module TestFoo;
    Foo foo;
    Reg#(Bit#(32)) cycle(0);
    rule tick;
        cycle <= cycle + 1;
        $display(foo.m);
        if (cycle > 10) $finish;
    endrule
endmodule

