#!/bin/sh -e

ERROR=${1:?Missing command line parameter}
FN=${1%%.tex}

for i in 1 2 3
do
    pdflatex -interaction batchmode $FN.tex
    grep -q 'undefined references' $FN.log && continue
    break
done

true
