MAIN=sesh.tex
OUTPUT=$(MAIN:.tex=.pdf)
DEPS=$(MAIN:.tex=.d)
PDFLATEX=pdflatex -halt-on-error
SED=sed
GENFIGS=fig/time_est.eps fig/time_recon.eps

all: $(OUTPUT)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SED=gsed
endif

$(DEPS): $(MAIN) $(GENFIGS) Makefile

# run latexmk, and supress a lot of the output, and color based on error level.
# Post-process the deps file to allow moving and deleting dependencies
$(OUTPUT): $(MAIN) $(GENFIGS) Makefile
	@tput setaf 15
	@echo "[latexmk] $@"
	@tput setaf 12
	@set -o pipefail;\
	OUT=$$(latexmk -pdflatex="$(PDFLATEX)" -pdf -M -MF $(DEPS) $< |& sed -n -e '/^!/,+1p' -e 's/Latexmk: //p' -e '/  Citation/p' | sed -e 's/^/[latexmk] /g' | grep -v 'This is');\
		S=$${PIPESTATUS[0]};\
		if [[ "$$S" == "0" ]]; then \
			V=12;\
			if (echo "$$OUT" | grep '  Citation') >/dev/null ; then \
				V=11;\
			fi;\
			tput setaf $$V;\
			echo "$$OUT";\
		else \
			tput setaf 9;\
			echo "$$OUT";\
		fi;\
		tput setaf 15;\
		test $$S -eq 0
	@echo "[sed]     $(DEPS)"
	@$(SED) -i -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
            -e '/^$$/ d' -e 's/$$/ :/' $(DEPS)

fig/time_est.eps: establish.txt

fig/time_recon.eps: reconn.txt

fig:
	@mkdir -p fig

fig/%.eps: %.py fig
	@tput setaf 15
	@echo "[python]  $@"
	@tput setaf 11
	@python $< |& sed -e 's/^/[python]  /g'
	@tput setaf 15

establish.txt: client_data server_data Makefile
	@echo "[pilot]   $@"
	@OUT=$$(grep 'BENCH client con' client_data | awk '{print $$NF}' | pilot analyze --preset=normal -);\
		(grep mean <<< $$OUT | awk '{print $$NF}' | tr '\n' ' ';\
		grep CI <<< $$OUT | awk '{print $$NF}') > establish.txt
	@OUT=$$(grep -a 'BENCH server acc' server_data | awk '{print $$NF}' | pilot analyze --preset=normal -);\
		(grep mean <<< $$OUT | awk '{print $$NF}' | tr '\n' ' ';\
		grep CI <<< $$OUT | awk '{print $$NF}') >> establish.txt

reconn.txt: client_data server_data Makefile
	@echo "[pilot]   $@"
	@OUT=$$(grep 'BENCH re' client_data | awk '{print $$NF}' | pilot analyze --preset=normal -);\
		(grep mean <<< $$OUT | awk '{print $$NF}' | tr '\n' ' ';\
		grep CI <<< $$OUT | awk '{print $$NF}') > reconn.txt
	@OUT=$$(grep -a 'BENCH server re' server_data | awk '{print $$NF}' | pilot analyze --preset=normal -);\
		(grep mean <<< $$OUT | awk '{print $$NF}' | tr '\n' ' ';\
		grep CI <<< $$OUT | awk '{print $$NF}') >> reconn.txt


clean:
	-rm $(DEPS) $(GENFIGS) $(OUTPUT) *.bbl *.aux *.blg *.fdb_latexmk *.fls *.log *.out
	-rm establish.txt reconn.txt

-include $(DEPS)

