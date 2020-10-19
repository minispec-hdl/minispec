function Bit#(1) sel(Bit#(4) a);
    Integer j = 0;
    Bool got_in = False;
    for (Integer i = 0; i < 4; i = i+1) begin
        if (i % 2 == 0) begin
            j = i;
            got_in = True;
        end
    end
    Integer k = j;
    if (got_in) begin
        return ~a[k];
    end else return 0;
endfunction
