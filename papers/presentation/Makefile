# slides.org -> Beamer/LaTeX (ox-beamer), not Typst.
# Needs: emacs with org, latex with beamer, metropolis, tgheros, textpos, booktabs, tikz.
# Debian: texlive-latex-extra texlive-fonts-extra texlive-pictures

NAME=slides

all: $(NAME).pdf

%.pdf: %.tex
	pdflatex -shell-escape -interaction nonstopmode $*.tex
	pdflatex -shell-escape -interaction nonstopmode $*.tex

%.tex: %.org
	emacs -batch --eval "(require (quote ox-beamer))" $< --funcall org-beamer-export-to-latex

clean:
	rm -f $(NAME).aux $(NAME).log $(NAME).nav $(NAME).out $(NAME).snm $(NAME).toc $(NAME).vrb

.PHONY: all clean
