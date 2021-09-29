module Test;
    rule test;
        Integer vi = 0;
        Bit#(8) vb = 0;
        for (Integer i = 0; i < 8; i = i + 1) begin
            if (i % 3 == 1) begin
                vi = vi + 1;
                vb = vb + 1;
            end
        end
        if (vi != vb) begin
            $display("Test 1 FAIL %d != %d", vi, vb);
            $finish;
        end

        for (Integer i = 0; i < 8; i = i + 1) begin
            case (i)
                0: begin
                    vi = vi + 7;
                    vb = vb + 7;
                end
                1: begin
                    vi = vi + 3;
                    vb = vb + 3;
                end
                default: begin
                    vi = vi + 2;
                    vb = vb + 2;
                end
            endcase
        end
        if (vi != vb) begin
            $display("Test 2 FAIL %d != %d", vi, vb);
            $finish;
        end
        
        for (Integer i = 0; i < 4; i = i + 1) begin
            vi = case (i % 2)
                0: vi + 1;
                1: vi + 10;
            endcase;
            vb = case (i % 2)
                0: vb + 1;
                1: vb + 10;
            endcase;
        end
        if (vi != vb) begin
            $display("Test 3 FAIL %d != %d", vi, vb);
            $finish;
        end

        $display("PASS %d == %d", vi, vb);
        $finish;
    endrule
endmodule
