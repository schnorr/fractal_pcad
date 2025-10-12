all:
	echo "specify a target"

%.pdf: %.org
	emacs -batch $^ --funcall org-babel-tangle
	emacs -batch $^ --eval "(setq org-latex-listings 'minted org-latex-packages-alist '((\"\" \"minted\")) org-latex-pdf-process '(\"pdflatex -shell-escape -interaction nonstopmode -output-directory %o %f\" \"pdflatex -shell-escape -interaction nonstopmode -output-directory %o %f\"))" --eval "(setq org-latex-minted-options '((\"frame\" \"lines\") (\"fontsize\" \"\\\\scriptsize\")))" --funcall org-beamer-export-to-pdf
