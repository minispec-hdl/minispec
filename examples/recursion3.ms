// Compiling this correctly requires static elaboration of ternary statement
// (f#(m-1,n-1) must be emitted only if n != 0 and m != 0)
function Bit#(64) f#(Integer m, Integer n) =
    (n == 0)? m : (m == 0)? n : (n + m) * f#(m-1,n-1);

module TestRecursion;
    method Bit#(64) res = f#(10,4);
    rule test;
        $display("%d %d\n", f#(10,4), f#(20,25));
        $finish;
    endrule
endmodule
