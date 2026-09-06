NAME=slides

all: $(NAME).pdf

%.pdf: %.tex
	pdflatex -shell-escape -interaction nonstopmode $*.tex
	pdflatex -shell-escape -interaction nonstopmode $*.tex

%.tex: %.org
	emacs -batch $< --funcall org-beamer-export-to-latex

clean:
	rm -f $(NAME).aux $(NAME).log $(NAME).nav $(NAME).out $(NAME).snm $(NAME).toc $(NAME).vrb

.PHONY: all clean
