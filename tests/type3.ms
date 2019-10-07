typedef enum {A, B, C} TestEnum;
typedef struct {Bool a; Bool b;} TestStruct;

function Maybe#(TestEnum) foo() = Valid(True);

function Bool bar(Maybe#(TestEnum) x) = (fromMaybe(0, x) == A);

function Bool baz(Maybe#(TestStruct) x) = fromMaybe(0, x).a;

function Bool qux(Maybe#(TestStruct) x);
    // Tests that typeclass is parsed correctly on parametric types.
    // e.g., Literal#(Maybe#(TestStruct)) --> Literal, not Literal#(Maybe
    let y = Valid(fromMaybe(x, Valid(x)));
    let a = fromMaybe(0, y);
    return a == A;
endfunction
