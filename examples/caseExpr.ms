function Bit#(2) f(Bit#(1) a, Bit#(1) b, Bit#(1) c);
    Bit#(4) u = 4'hB; // hex value 0xB
    Bit#(4) l = (c == 1)? 4'h8 : 4'h7;
    Bit#(8) x = {u, l};
    Bit#(2) ret = case ({a,b})
        0: 'h1;  // this branch is elaborated (to test exitCaseExprItem when getValue is non-null)
        1: x[1:0];
        2: x[3:2];
        3: x[7:6] ^ 2'b11;  // this doesn't work in Bluespec, but does in Minispec (see exitCaseExprItem)
    endcase;
    return ret;
endfunction
