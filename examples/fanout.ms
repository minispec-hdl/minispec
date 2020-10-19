function Bit#(n) mux#(Integer n)(Bit#(n) a, Bit#(n) b, Bool sel) = sel ? a : b;
