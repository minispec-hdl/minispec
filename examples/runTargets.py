# Used for automated regression testing --- see tests/run.py

compileTargets = [
    "bsvimport",
    "typedecl",
]

simTargets = [
    ("cmp", "TestCmp"),
    ("counter", "TestCounter"),
    ("import", "TestImports"),
    ("loop", "TestAdd"),
    ("loop2", "TestCmp"),
    ("params", "TestParams"),
    ("partialparams", "TestPartialParams"),
    ("recursion", "TestAdd"),
    ("recursion2", "TestAdd"),
    ("recursion3", "TestRecursion"),
    ("sharedcounter", "TestSharedCounter"),
    ("tree", "TestLessThan"),
    ("typeparams", "TestTypeParams"),
]

synthTargets = [
    ("caseExpr", "f"),
    ("cmp", "cmp#(11)"),
    ("counter", "DeltaCounter"),
    ("elabnested", "sel"),
    ("elabsimple", "add8#(9)"),
    ("fanout", "mux#(64)"),
    ("intliterals", "add"),
    ("loop", "add#(27)"),
    ("loop2", "cmp_loop#(17)"),
    ("mfparams", "MultiFileParametrics"),
    ("nonewline2", "foo"),
    ("nonewline2", "bar"),
    ("simplecounter", "Counter"),
    ("simple", "foo"),
]

