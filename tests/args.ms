
function Bool foo = True;

function Bool bar(Bool x) = !x;

function Bool baz(Bool x, Bool y) = !x && !y;

module Test;
    rule test;
        Bool a = foo;
        Bool b = foo(True);
        Bool c = foo(True, True);
        
        Bool d = bar;
        Bool e = bar(True);
        Bool f = bar(True, True);

        Bool g = baz;
        Bool h = baz(True);
        Bool i = baz(True, True);

        Bool j = bar(1);
        Bool k = baz(1, 2);
    endrule
endmodule
