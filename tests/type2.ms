function Bit#(n+1) add#(Integer n)(Bit#(n) x, Bit#(n) y);
    if (n == 1) return x ^ y;
    else begin
        Bit#(n+1) s = add#(n-1)(x[n-2:0], x[n-2:0]);
        return {(s > x)? 0 : 1, s};
    end
endfunction

function Bit#(n) foo#(Integer n, type K) = 5'd0;

function Bit#(9) f9 = foo#(4, Bit);

module TestAdd;
    method Bit#(9) res = add#(8)(3, 4);

    rule test;
        for (Integer i = 4; i < 10; i = i + 1) begin
            Bit#(i) x = add#(i)(i, i+1);
        end
    endrule
endmodule
