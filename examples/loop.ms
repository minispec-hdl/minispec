function Bit#(w) add#(Integer w)(Bit#(w) a, Bit#(w) b);
    Bit#(w+1) cin = 0;
    Bit#(w) res = 0;
    for (Integer i = 0; i < w; i = i + 1) begin
        let sum = a[i] ^ b[i] ^ cin[i];
        let cout = (a[i] & b[i]) | (a[i] & cin[i]) | (b[i] & cin[i]);
        res[i] = sum;
        cin[i+1] = cout;
    end
    return res;
endfunction

module TestAdd;
    Reg#(Bit#(16)) cycle(0);
    rule test;
        cycle <= cycle + 1;
        Bit#(8) a = cycle[15:8];
        Bit#(8) b = cycle[7:0];
        
        Bit#(8) res = add#( 8 )(a, b);
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
