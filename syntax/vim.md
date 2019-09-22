Minispec syntax for vim
=======================

Since Minispec's syntax is very similar to BSV's, you can use BSV's syntax coloring and indentation plugins for vim.

The following instructions assume you are in a system with a working Bluespec compiler installation, with `$BLUESPECDIR` set (e.g., on Athena with the 6.004 setup done).
If you want to install these in a machine without Bluespec, you'll have to copy the files under `$BLUESPECDIR` to your machine.

The following instructions will set up vim for both Minispec and BSV.

1. Ensure the `~/.vim/indent` and `~/.vim/syntax` folders exist, and create them otherwise:
```bash
mkdir -p ~/.vim/indent
mkdir -p ~/.vim/syntax
```
2. Copy the BSV plugins for indentation and syntax from `$BLUESPECDIR`:
```bash
cd $BLUESPECDIR/../util/vim/indent/bsv.vim ~/.vim/indent/
cd $BLUESPECDIR/../util/vim/syntax/bsv.vim ~/.vim/syntax/
```
3. Add the folowing lines to your `.vimrc`:
```vim
filetype plugin indent on
autocmd BufNewFile,BufRead *.bsv set ft=bsv
autocmd BufNewFile,BufRead *.ms set ft=bsv
autocmd FileType bsv set shiftwidth=4 expandtab smarttab
let b:verilog_indent_modules = 1
```

If you already have vim set up with BSV syntax, you only need step 3.
