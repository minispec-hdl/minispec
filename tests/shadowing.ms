function Bit#(1) shadows(Bit#(1) in);
    let x = in;
    if (True) let x = ~in;
    if (!False) begin let x = in; end
    if (~in == 0) let x = in;
    if (in == 1) let x = in;
    return x;
endfunction
