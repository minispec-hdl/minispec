import counter;

module TestImports;
    Counter c0(4);
    Counter c1(0);
    DeltaCounter dc;
    Reg#(Bit#(32)) cycle(0);

    rule test;
        $display("[cycle %d] c0 %d c1 %d dc %d", cycle, c0.getCount, c1.getCount, dc.getCount);
        cycle <= cycle + 1;
        c0.enable = (cycle[0] == 1);
        c1.enable = (cycle[1] == 1);
        if (cycle[1:0] != 0) dc.delta = zeroExtend(cycle[1:0]);
        if (cycle >= 1024) $finish;
    endrule
endmodule
