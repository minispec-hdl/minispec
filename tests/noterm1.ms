function Bit#(w+1) add#(Integer w)(Bit#(w) a, Bit#(w) b);
    let x = add#(w-1)(a[w-2:0], b[w-2:0]);
    let sum = x[w-1] ^ a[w-1] ^ b[w-1];
    let carry = (x[w-1] & (a[w-1] | b[w-1])) | (a[w-1] & b[w-1]);
    return {carry, sum, x[w-2:0]};
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
