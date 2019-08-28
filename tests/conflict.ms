module Counter(Bit#(8) initialValue);
    Reg#(Bit#(8)) count(initialValue);
    method Bit#(8) getCount = count;
    input Bool enable default = False;
    rule inc;
        if (enable) count <= count + 1;
    endrule
endmodule

module CounterUser(Bit#(8) _id, Reg#(Bit#(32)) _cycle, Counter _counter);
    let id = _id;
    Reg#(Bit#(32)) cycle = _cycle;
    // FIXME: This should be detected as a module!
    Counter counter = _counter;

    rule count;
        // This will not compile if there is more than one user (even with the if statement)
        if (truncate(cycle) == _id) _counter.enable = True;
        $display("CounterUser%d count %d", id, counter.getCount());
    endrule
endmodule

module TestSharedCounter;
    Reg#(Bit#(32)) cycle(0);
    Counter counter(0);
    CounterUser u0(0, cycle, counter);    
    CounterUser u1(1, cycle, counter);
    rule limit;
        cycle <= cycle + 1;
        if (cycle > 1024) $finish;
    endrule
endmodule


