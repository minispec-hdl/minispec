bsvimport Bsverror;

module Foo;
    rule tick;
        $display("res ",  foo(2, 3));
        $finish;
    endrule
endmodule
