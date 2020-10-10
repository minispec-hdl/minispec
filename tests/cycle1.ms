module Foo;
    input Bool in default = False;
    method Bool out = in;
endmodule

module Bar;
    Foo f;
    rule test;
        $display(f.out);
        f.in = True;
        $finish;
    endrule
endmodule

