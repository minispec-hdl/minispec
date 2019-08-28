function Bool foo(Bool a);
    // This must fail as it's legit poisoning
    Integer i;
    if (a) i = 1;
    else i = 2;
    Integer j = i + 1;

    // This may fail depending on the compiler
    Integer n = 0;
    if (True) n = 1;
    Integer m = n + 1;
endfunction
