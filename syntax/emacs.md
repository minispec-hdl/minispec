Minispec syntax for emacs
=========================

Since Minispec's syntax is very similar to BSV's (and mostly a subset), you can use BSV's syntax coloring and indentation plugins for emacs.

The following instructions assume you are in a system with a working Bluespec compiler installation, with `$BLUESPECDIR` set (e.g., on Athena with the 6.004 setup done).
If you want to install these in a machine without Bluespec, you'll have to copy the files under `$BLUESPECDIR` to your machine.

The following instructions will set up emacs for both Minispec and BSV.

1. Create a local folder to store the emacs bsv-mode files:
```bash
mkdir -p ~/.emacs.d/bsv
```
2. Copy the bsv-mode files from `$BLUESPECDIR`:
```bash
cp $BLUESPECDIR/../util/emacs/*.el ~/.emacs.d/bsv/
```
3. Add the following lines to your `~/.emacs`:
```emacs
(add-to-list 'load-path "~/.emacs.d/bsv/")
(autoload 'bsv-mode "bsv-mode" "BSV mode" t )
(setq auto-mode-alist (cons  '("\\.bsv\\'" . bsv-mode) auto-mode-alist))
(setq auto-mode-alist (cons  '("\\.ms\\'" . bsv-mode) auto-mode-alist))
```

If you already have vim set up with BSV syntax, you only need to add the last line of step 3.
