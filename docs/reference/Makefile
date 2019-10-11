# Halt on errors instead of going to the shell
LATEX_OPTS=-halt-on-error -mode=batch -shell-escape

TGT=reference
default: $(TGT)

figs:
	make -C figures/

$(TGT): figs
	date --iso-8601 > auto_version.tex
	echo " from source version " >> auto_version.tex
	../../misc/gitver.py >> auto_version.tex
	pdflatex $(LATEX_OPTS) $(TGT)
	pdflatex $(LATEX_OPTS) $(TGT)
	pdflatex $(LATEX_OPTS) $(TGT)
	ps2pdf -dPDFSETTINGS=/prepress -dCompatibilityLevel=1.4 -dEmbedAllFonts=true -dSubsetFonts=true -r600 $(TGT).pdf $(TGT)_opt.pdf
	# Preserve metadata
	pdftk $(TGT).pdf dump_data output metadata.txt
	pdftk $(TGT)_opt.pdf update_info metadata.txt output $(TGT).pdf
	#rm $(TGT)_opt.pdf

tidy:
	make -C figures clean
	rm -f auto_version.tex *.aux *.blg *.bbl *.log *.brf *.dvi *.ps metadata.txt

clean: tidy
	rm -f $(TGT).pdf
