
typedef Int#(n) SignedInt#(Integer n);

typedef Maybe#(Bit#(k)) MBit#(Integer k);

typedef struct {
    Bit#(n) a;
    Bit#(2*n) b;
} BitStruct#(Integer n);

function BitStruct#(n) foo#(Integer n)(MBit#(n) in) = BitStruct#(n){ a: n*fromMaybe(?, in), b: {fromMaybe(?, in), 0} };

module Test#(Integer n);
    RegU#(BitStruct#(n/3)) r;
    rule tick;
        r <= BitStruct#(n/3){a: truncate(r.b), b: r.b + signExtend(r.a)};
    endrule

    method Bit#(n) getSomething = foo#(n)(Valid(pack(r))).a;
endmodule

module TestParams;
    Test#(3) x;
    Test#(6) y;
    Test#(9) z;
    Reg#(Bit#(4)) cycle(0);
    rule tick;
        $display(cycle, ": ", x.getSomething, " | ", y.getSomething, " | ", z.getSomething);
        if (cycle == 15) $finish;
        cycle <= cycle + 1;
    endrule
endmodule
