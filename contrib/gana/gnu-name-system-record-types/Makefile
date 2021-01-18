FILES=gnu-name-system-record-types.h
all: check $(FILES)

check:
	recfix --check registry.rec

distclean:

	rm -f *.tmp
clean:
	rm -f $(FILES) *.tmp

gnu-name-system-record-types.h.tmp: registry.rec h.template
	../format.sh h.template < registry.rec > $@

gnu-name-system-record-types.h: h.header gnu-name-system-record-types.h.tmp h.footer
	cat $^ > $@

.PHONY: check clean distclean
