Minispec syntax highlighting for nano
=====================================

We expect most people to use more advanced editors than nano (e.g., vim or emacs) to write Minispec code. However, you may find nano useful to view Minispec code or for some basic editing.

To install this file:
1. Copy minispec.nanorc to your nano config directory (e.g., `cp minispec.nanorc ~/.nano/`). If you do not have a nano config directory, create it first (e.g., `mkdir ~/.nano/`).
2. Add the following line to your `~/.nanorc`: `include "~/.nano/minispec.nanorc"`

We recommend you also add the following lines to your .nanorc:
```
set autoindent
set tabsize 4
set tabstospaces
```
