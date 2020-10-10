module Conflict;
    Reg#(Bool) a(False);

    input Bool b;

    rule tick;
        if (a && b) a <= True;
        /*if (!a || b)*/ a <= False;
    endrule

    method Bool x = a;
endmodule
