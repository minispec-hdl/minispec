module Counter;
    Reg#(Bit#(2)) count(0);
    input Bool inc;
    method Bit#(2) getCount = count;
    rule tick;
        if (inc) count <= count + 1;
    endrule
endmodule
