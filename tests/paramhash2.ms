// Sometimes args and params get confused...
// TODO: This fails on use, but is parsed because the grammar accepts Bool as a paramFormal type (to avoid making Integer a keyword).
function Bool foo#(Bool x);
    Bit#(2) a = x? 3 : 2;
    Vector#(2, Bit(2)) v;
    v[0] = a;
    v[1] = ~a;
    return v[1][0];
endfunction
