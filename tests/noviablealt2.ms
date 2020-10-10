module Foo;
    Reg#(Bit#(2)) a(1);
    Reg#(Bit#(2)) b(2);
    rule x;
        a <= a + 1;
        if (a == 0) begin
            $display("l1")  // error, missing ";"
            $display("l2");
            if (b == 0)
                $display("l3");
            
        end
    endrule

    method Bit#(1) xx = a[0];
endmodule

