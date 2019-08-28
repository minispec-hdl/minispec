function Bit#(1)f(Bit#(1)a,Bit#(1)b);
    Integer p = 5 &&    3;
    Integer q = 5 && 
    3;
    Integer xxxxx =    !4;
    Integer c;
    Bit#(1) d;
    Integer m = a + b - c * d;
    let x = a ^ b;
    Integer g = 2;
    Integer h = -3;
    Integer j = (g > h)? 2*g : a + 36;  // should be OK
    Integer j = (True)? 2*g : a + 36;  // should be OK
    Integer k = (g < h)? 2*g : a + 36;  // should fail
    Integer l = (g + h)? 2*g : 42;  // should fail

    Integer l = (g + h
            + k)? 2*g : 42;  // should fail
    return x;
endfunction
