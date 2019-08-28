module BUF(A, Z);
input A;
output Z;
assign Z = A;
endmodule

module INV(A, ZN);
input A;
output ZN;
assign ZN = ~A;
endmodule

module DFF(D, CK, Q, QN);
output Q, QN;
input D, CK;
reg Q;
always @(posedge(CK)) Q = D;
assign QN = ~Q;
endmodule

module NAND2(A1, A2, ZN);
input A1, A2;
output ZN;
assign ZN = ~(A1 & A2);
endmodule

module NOR2(A1, A2, ZN);
input A1, A2;
output ZN;
assign ZN = ~(A1 | A2);
endmodule

module XNOR2(A, B, ZN);
input A, B;
output ZN;
assign ZN = ~(A ^ B);
endmodule

module NAND3(A1, A2, A3, ZN);
input A1, A2, A3;
output ZN;
assign ZN = ~(A1 & A2 & A3);
endmodule

module NOR3(A1, A2, A3, ZN);
input A1, A2, A3;
output ZN;
assign ZN = ~(A1 | A2 | A3);
endmodule

module NAND4(A1, A2, A3, A4, ZN);
input A1, A2, A3, A4;
output ZN;
assign ZN = ~(A1 & A2 & A3 & A4);
endmodule

module NOR4(A1, A2, A3, A4, ZN);
input A1, A2, A3, A4;
output ZN;
assign ZN = ~(A1 | A2 | A3 | A4);
endmodule

// Non-invering variants
module AND2(A1, A2, ZN);
input A1, A2;
output ZN;
assign ZN = (A1 & A2);
endmodule

module OR2(A1, A2, ZN);
input A1, A2;
output ZN;
assign ZN = (A1 | A2);
endmodule

module XOR2(A, B, Z);
input A, B;
output Z;
assign Z = (A ^ B);
endmodule

module AND3(A1, A2, A3, ZN);
input A1, A2, A3;
output ZN;
assign ZN = (A1 & A2 & A3);
endmodule

module OR3(A1, A2, A3, ZN);
input A1, A2, A3;
output ZN;
assign ZN = (A1 | A2 | A3);
endmodule

module AND4(A1, A2, A3, A4, ZN);
input A1, A2, A3, A4;
output ZN;
assign ZN = (A1 & A2 & A3 & A4);
endmodule

module OR4(A1, A2, A3, A4, ZN);
input A1, A2, A3, A4;
output ZN;
assign ZN = (A1 | A2 | A3 | A4);
endmodule
