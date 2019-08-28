bsvimport Vector;

function Bool foo = False;

function Bit#(2) bar(Bit#(2) a) = a + 3;

function Bool f(Bit#(1) a, Bit#(1) b);
    a = a + 1;
    let x = a ^ b;
    if (a == b)
    let x = z;
    //return x == True;
    Bool l = 1;
    Bool m = (1 + x);
    Bool n = (True + x);
    Bool o = (1 > l);
    Bool p = (True > x);
    Bit#(2) q = a && b;
    Bool r = a && b;
    Vector#(4, Bit#(32)) v = True;
    Vector#(8, Bit#(32)) vv = foo();
    Bit#(1) aa = bar(a);
    Bit#(2) aaa = bar(a);
    return x && b;
endfunction

module TestX;
    Reg#(Bit#(32)) cycle(0);
    method Bool out = f(cycle[0], cycle[1]);
    rule test;
        cycle <= cycle + 1;
    endrule
endmodule
        

