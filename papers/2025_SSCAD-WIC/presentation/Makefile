NAME=slides

all: $(NAME).pdf

%.pdf: %.tex
	pdflatex -shell-escape -interaction nonstopmode $*.tex
	bibtex $*
	pdflatex -shell-escape -interaction nonstopmode $*.tex
	pdflatex -shell-escape -interaction nonstopmode $*.tex

%.tex: %.org
	emacs -batch $< --funcall org-babel-tangle
	emacs -batch $< --eval "(setq org-latex-listings 'minted org-latex-packages-alist '((\"\" \"minted\")))" --eval "(setq org-latex-minted-options '((\"frame\" \"lines\") (\"fontsize\" \"\\\\scriptsize\")))" --funcall org-beamer-export-to-latex