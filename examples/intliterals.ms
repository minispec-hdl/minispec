// Test different ways to write sized/unsized integer literals
function Bit#(32) add(Bit#(32) a) = a + 'b1000_0000 + 32'b101 + 'hBEEF + 32'hFFDF + 42 + 'd4_2 + 32'd97;
