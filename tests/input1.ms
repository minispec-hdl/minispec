// Wrong default
module Foo;
    input Bool x default = 1;
endmodule

// Our own input doesn't exist (should just be an unknown variable error)
module Bar;
    rule test;
        Bit#(1) x = in;
        $display(x);
    endrule
endmodule

// Submodule input doesn't exist - T0016
module Sub;
    input Bool in default = False;
    rule test;
        Bit#(1) x = in;
        $display(x);
    endrule
endmodule

module Setter;
    Sub sub;
    rule test;
        sub.inp = True;
    endrule
endmodule

// Submodule input set with wrong type
module Setter2;
    Sub sub;
    rule test;
        Bit#(1) x = 1;
        sub.in = x;
    endrule
endmodule
