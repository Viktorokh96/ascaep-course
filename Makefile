all: draft.pdf

draft.pdf : okhotnikov.pdf

example.pdf : masterthes.pdf bachelor.pdf diploma.pdf coursreport.pdf projreport.pdf thesis.pdf
	gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -sOutputFile=$@ $^

%.pdf : %.tex
	./makepdf.sh $<

.PHONY : clean

clean : 
	rm *.pdf && rm *.log && rm *.aux && rm *.toc
