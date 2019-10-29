// Test that type errors are handled correctly in a file with tabs. bsc treats
// tabs as a variable number of spaces, which messes up the sourcemap.
// NOTE: Do not edit with an editor that turns tabs to spaces!
function Bool foo =     	1;
