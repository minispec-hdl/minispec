function Bit#(w+1) add#(Integer w)(Bit#(w) a, Bit#(w) b);
    Bit#(w+1) out = 0;
    for (Integer i = 0; i < w*w*w*w*w*w*w; i = i + 1)
        out[i % (w+1)] = a[0] ^ b[0];
    return out;
endfunction

module TestAdd;
    Reg#(Bit#(16)) cycle(0);
    rule test;
        cycle <= cycle + 1;
        Bit#(8) a = cycle[15:8];
        Bit#(8) b = cycle[7:0];
        
        Bit#(8) res = truncate(add#( 8 )(a, b));
        Bit#(8) expected = a + b;
        
        if (res != expected) begin
            $display("FAIL: add(%d, %d) = %d, expected %d", a, b, res, expected);
            $finish();
        end
        if (cycle == -1) begin
            $display("PASS");
            $finish();
        end
    endrule
endmodule
