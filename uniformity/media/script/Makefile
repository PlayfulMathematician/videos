
MAIN = main
all: $(MAIN).pdf
$(MAIN).pdf: $(MAIN).tex $(MAIN).bib
	pdflatex $(MAIN).tex
	biber $(MAIN)
	pdflatex $(MAIN).tex
	pdflatex $(MAIN).tex
clean:
	rm -f *.aux *.bbl *.blg *.log *.out *toc *.bcf *.xml *.ent

distclean: clean
	rm -f $(MAIN).pdf

