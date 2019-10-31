// Tests for wrong ways to declare registers (and more generally, submodules)

module Foo;
    Reg#(Integer) unsynth(0);
    method Bit#(1) out = unsynth[0];
endmodule

module Bar;
    Vector#(1, Reg#(Integer)) x(0);
endmodule

module RegReg;
    Reg#(Reg#(Bool)) x(True);
endmodule

module RegURegU;
    RegU#(RegU#(Bool)) x;
endmodule

// This triggers a Monad#() proviso error... hoping noone will ever stumble upon this.
module VRegReg;
    Vector#(1, Reg#(Reg#(Bool))) x;
endmodule

module Baz;
    Vector#(1, Reg#(Integer)) y;
endmodule

module Qux;
    Vector#(1, Reg#(Bool)) z;
endmodule
