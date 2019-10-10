// Double set, which should be illegal - G0004
module Sub;
    input Bool in default = False;
    rule test;
        Bit#(1) x = pack(in);
        $display(x);
    endrule
endmodule

module Setter;
    Sub sub;
    rule test;
        sub.in = True;
        sub.in = False;
    endrule
endmodule

// No-default variant - same, G0004
module Sub2;
    input Bool in;
    rule test;
        Bit#(1) x = pack(in);
        $display(x);
    endrule
endmodule

module Setter2;
    Sub2 sub;
    rule test;
        sub.in = True;
        sub.in = False;
    endrule
endmodule

// Never setting an input without a default - G0066
module NoSetter;
    Sub2 sub;
    rule test;
        $display("What did I forget now...");
    endrule
endmodule

// Not always setting an input without a default - G0015, which is a WARNING (should be promoted an error)
module SometimesSetter;
    Sub2 sub;
    Reg#(Bool) b(False);
    rule test;
        b <= !b;
        if (b)
            sub.in = True;
        else
            $display("What did I forget now...");
    endrule
endmodule
