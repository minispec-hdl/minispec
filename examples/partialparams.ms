// Example showing partial parametric specialization

typedef Bit#(64) Word;

// Partially specialized parametric are allowed, and have higher priority the
// more spezialized params they have. Among defs with the same number of
// specialized params, the earlier def wins. So calls to foo#(x, y) have
// priorities:
// 1. foo#(0, 0)
// 2. foo#(0, Integer y)
// 3. foo#(Integer x, 0)
// 4. foo#(Integer x, Integer y)
function Word foo#(Integer x, Integer y) = foo#(x-1,y) * 2;
function Word foo#(0, Integer y) = foo#(y+1,y/2) + 1;
function Word foo#(Integer x, 0) = foo#(x/2,x/3) + 2;
function Word foo#(0, 0) = 7;

// You may also have parametrics with other type/Integer combinations; there's
// no conflict (though in this case, these are not used)
function Word foo#(type X, Integer y) = 42;
function Word foo#(Bool, Integer y) = 32;

module TestPartialParams;
    rule tick;
        $display(foo#(4,7));
        $display(foo#(7,4));
        $finish;
    endrule
endmodule

