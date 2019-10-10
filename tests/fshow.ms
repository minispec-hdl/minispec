// Tests bsc error processing for T0031 fshow typeclass
module Foo;
    Reg#(Bool) r(True);
    rule test;
        // All OK
        Integer i = 3;
        $display(fshow(i));
        $display(fshow(r));

        // Bad
        let x = isValid;
        $display(fshow(x));
    endrule
endmodule

module Bar;
    Vector#(4, Reg#(Bool)) v(True);
    rule test;
        // Bad, T0032
        $display(fshow(v));
    endrule
endmodule
