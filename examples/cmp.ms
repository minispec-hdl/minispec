typedef struct {
    Bool eq;
    Bool lt;
} CmpRes;

function CmpRes cmp#(Integer w)(Bit#(w) a, Bit#(w) b);
    CmpRes hi = cmp#(w-w/2)(a[w-1:w/2], b[w-1:w/2]);
    CmpRes lo = cmp#(w/2)(a[w/2-1:0], b[w/2-1:0]);
    return CmpRes{eq: hi.eq && lo.eq, lt: hi.lt || (hi.eq && lo.lt)};
endfunction

function CmpRes cmp#(1)(Bit#(1) a, Bit#(1) b);
    return CmpRes{eq: (a ^ b) == 0, lt: (~a & b) == 1};
endfunction

module TestCmp;
    Reg#(Bit#(18)) cycle(0);
    rule test;
        cycle <= cycle + 1;
        Bit#(9) a = cycle[17:9];
        Bit#(9) b = cycle[8:0];
        
        let res = cmp#(9)(a, b);
        let expected = CmpRes{eq: a == b, lt: a < b};
        
        if (res != expected) begin
            $display("FAIL: lessThan(%d, %d) = ", a, b, fshow(res), ", expected ", fshow(expected));
            $finish();
        end
        if (cycle == -1) begin
            $display("PASS");
            $finish();
        end
    endrule
endmodule
