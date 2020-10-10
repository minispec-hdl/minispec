module Test;
    Reg#(Bool) x(False);
    rule test;
        x <= False;
        x <= True;
        x <= False;
    endrule
endmodule

