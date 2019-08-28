function Bool foo#(Integer i)(Bool a) = a && (i % 2 == 0);

function K#(i) bar#(Integer i, type K)(Bool a) = (i % 2 == 0);

module Test;
    rule tick;
        // only a, l should compile
        let a = foo#(2)(True);
        let b = foo#(Bit#(1))(a);
        let c = foo#(2,Bit)(a);
        let d = foo#(Bit,2)(a);
        
        let l = bar#(7, Bit)(True);
        let m = bar#(Bit, 7)(True);
        let n = bar#(7)(True);
        let o = bar#(7, Bit, 7)(True);
    endrule
endmodule
