module Foo;
    rule test;
        Bit#(2) a = 1;
        Bit#(4) b = 2;
        Bit#(5) c = {a, b, 0};
        Bit#(7) d = truncate(b);
    endrule
endmodule
