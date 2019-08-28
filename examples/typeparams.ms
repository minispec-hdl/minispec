function X add#(type X, Integer n)(X a, X b) = a + b + n;

typedef struct {
    X a;
} Rec#(type X);

function X addrec#(type X, Integer n)(Rec#(X) a, Rec#(X) b) = a.a + b.a + n;

module T#(type X, type Y, Integer n);
    Integer k = (n <= 1)? 1 : n - 1;
    T#(X, Y, k) s;
    X a(unpack(0));
    Y#(2*k) b = 4;
    method Y#(2*k) get = b;
endmodule

// Base case (avoid infinite recursion...)
module T#(Reg#(Rec#(Bit#(4))), Bit, 1);
endmodule

module TestTypeParams;
    RegU#(Maybe#(Rec#(Rec#(Bit#(4))))) thing;
    Reg#(Maybe#(Rec#(Rec#(Bit#(4))))) thing2(Invalid);
    T#(Reg#(Rec#(Bit#(4))), Bit, 8) kraken;
    rule test;
        Bit#(4) a = 1;
        Bit#(4) b = 4;
        let res = add#(Bit#(4), 4)(a, b);
        $display(res);

        Rec#(Bit#(8)) ra, rb;
        ra.a = 2;
        rb.a = 5;
        let res2 = addrec#(Bit#(8), 4)(ra, rb);
        $display(res2);
        
        $finish;
    endrule
endmodule
