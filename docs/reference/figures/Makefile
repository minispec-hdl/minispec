default: moves

figures-crop.pdf: figures.pdf Makefile
	pdfcrop figures.pdf 

doc_data.txt: figures-crop.pdf
	pdftk figures-crop.pdf burst output drawing_%02d.pdf

# Now for some fun Makefile hackery: pdfcrop generates a report called doc_data.txt. We depend on that report,
# not the drawing_XX.pdf files, so that those files can be deleted. Only clean, not tidy, kills that report 

moves: ag_fsm.pdf

#Need to name this as a file...
ag_fsm.pdf: doc_data.txt
	cp drawing_01.pdf ag_synthesis.pdf
	cp drawing_03.pdf ag_fsm.pdf
	cp drawing_04.pdf ag_basicmodule.pdf
	cp drawing_05.pdf ag_hiermodule.pdf
	cp drawing_06.pdf ag_badFsm.pdf
	cp drawing_07.pdf ag_reg.pdf

tidy:
	rm -f drawing_*.pdf

clean: tidy
	rm -f doc_data.txt
	rm -f ag_*.pdf
	rm -f figures-crop.pdf
