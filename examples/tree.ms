// NOTE: See the similar comparator test at cmp.ms
function Bool lessThan#(Integer w)(Bit#(w) a, Bit#(w) b);
    return lessThan#(w-w/2)(a[w-1:w/2], b[w-1:w/2])
        || ((a[w-1:w/2] == b[w-1:w/2]) && lessThan#(w/2)(a[w/2-1:0], b[w/2-1:0]));
endfunction

function Bool lessThan#(1)(Bit#(1) a, Bit#(1) b);
    return (~a & b) == 1;
endfunction

module TestLessThan;
    Reg#(Bit#(18)) cycle(0);
    rule test;
        cycle <= cycle + 1;
        Bit#(9) a = cycle[17:9];
        Bit#(9) b = cycle[8:0];
        
        Bool res = lessThan#(9)(a, b);
        Bool expected = a < b;
        
        if (res != expected) begin
            $display("FAIL: lessThan(%d, %d) = %d, expected %d", a, b, res, expected);
            $finish();
        end
        if (cycle == -1) begin
            $display("PASS");
            $finish();
        end
    endrule
endmodule
