function Bit#(32) barrelRShift(Bit#(32) in, Bit#(5) sftSz, Bit#(1) sft_in);
    Bit #(32) ret = 0;
    Bit #(32) n = 31 - sftSz;
    Bit #(sftSz) temp = signExtend(sft_in);
    Bit #(n) right = in[31:sftSz];
    ret = {temp, right};
    return ret;
endfunction
