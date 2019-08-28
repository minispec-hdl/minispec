Integer x = 8 + 1;
function Bit#(x+1) add8(Bit#(x) a, Bit#(x) b);
    Bit#(x/2+1) l = {0, a[3:0]} + {0, b[3:0]};
    Integer z = 2**(x+1);
    Integer y = log2(z) / 3; // 10 / 3 = 3
    Bit#((y-2)*4) u = a[7:x/2] + b[x-1:y+2] + zeroExtend(l[x/2]);
    l = l + 2**0;
    l = l + 2**1;
    l = l + 2**2;
    l = l + 2**3;
    let n = 4;
    Bit#(5) m = n + 3;
    for (Integer i = 0; i < 10; i = i + 1) u = u + 2**i;
    return {u, l[x/2+1:0]};
endfunction

