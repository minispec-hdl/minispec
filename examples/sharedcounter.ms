// NOTE: Multi-inputs with a parameterizable count are currently awkward; this
// shows how to use Wires and methods that return them as inputs. We could add
// vector inputs to avoid this, but I don't think this will show up in our use
// cases (of course, multiple inputs used by different modules are trivial)

Integer numUsers = 2;  // must be a power of 2
typedef Bit#(log2(numUsers)) UserId;

module SharedCounter;
    Reg#(Bit#(8)) counter(0);
    Vector#(numUsers, BypassWire#(Bool)) enables;
    method BypassWire#(Bool) getEnable(UserId i) = enables[i];
    method Bit#(8) getCount = counter;
    rule update;
        Bool enable = False;
        for (Integer i = 0; i < numUsers; i = i + 1)
            enable = enable || enables[i];
        if (enable) counter <= counter + 1;
    endrule
endmodule

module CounterUser(UserId id, Reg#(Bit#(32)) cycle, SharedCounter counter);
    rule count;
        let enable = counter.getEnable(id);
        enable <= (cycle[log2(numUsers)-1:0] == id);
        $display("CounterUser%d count %d", id, counter.getCount);
    endrule
endmodule

module TestSharedCounter;
    Reg#(Bit#(32)) cycle(0);
    SharedCounter counter;
    CounterUser u0(0, cycle, counter);
    CounterUser u1(1, cycle, counter);
    rule limit;
        cycle <= cycle + 1;
        if (cycle > 1024) $finish;
    endrule
endmodule
