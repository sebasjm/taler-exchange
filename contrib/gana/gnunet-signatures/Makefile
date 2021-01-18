FILES=registry.rec gnunet_signatures.h
all: check $(FILES)
check: registry.rec
	recfix --check registry.rec
distclean:
	rm -f *.tmp
clean:
	rm -f $(FILES) *.tmp
gnunet_signatures.h.tmp: registry.rec h.template
	../format.sh h.template < registry.rec > $@

gnunet_signatures.h: h.header gnunet_signatures.h.tmp h.footer
	cat $^ > $@

.PHONY: check clean distclean
