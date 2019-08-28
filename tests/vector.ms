bsvimport Vector;

module F;
    Vector v1;
    Vector#(1,2,3) v2;
    Vector#(Bool, 3) v3;
    Vector#(3, Bool) v4 = replicate(True);
endmodule
