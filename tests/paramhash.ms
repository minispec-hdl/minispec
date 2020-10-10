// A common error is to forget a hash in parametric types...
function Bool foo(Bool x);
    Bit#(2) a = x? 3 : 2;
    Vector#(2, Bit(2)) v;
    v[0] = a;
    v[1] = ~a;
    return v[1][0];
endfunction
