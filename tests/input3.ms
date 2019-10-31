module Foo;
    input Bool in;
endmodule

module Test1;
    Foo f;
    rule x;
        f.in = True;
    endrule
endmodule

/*module Test2;
    Foo f;
    rule x;
        f.in.in = True;
    endrule
endmodule*/

/*module Test3;
    Vector#(4, Foo) f;
    rule x;
        f.in[3] = True;
    endrule
endmodule*/

module Test4;
    Vector#(4, Foo) f;
    rule x;
        f[3].in = True;
    endrule
endmodule

/*module Test5;
    Foo f;
    rule x;
        f[3].in = True;
    endrule
endmodule*/

/*module Test6;
    Foo f;
    rule x;
        f[3:0].in = True;
    endrule
endmodule*/

module Test7;
    Vector#(4, Vector#(2, Vector#(6, Foo))) f;
    rule x;
        f[3][1][3].in = True;
    endrule
endmodule
