// Check nested Vector-submodule initialization is done properly
module Test;
    // Tensors, tensors everywhere!
    Vector#(4, Reg#(Bool)) v1(False);
    Vector#(4, Vector#(3, Reg#(Bool))) v2(False);
    Vector#(4, Vector#(3, Vector#(7, Reg#(Bool)))) v3(False);
    Vector#(4, Vector#(Vector#(7, Reg#(Bool)), 3)) v3bad(False);
endmodule
