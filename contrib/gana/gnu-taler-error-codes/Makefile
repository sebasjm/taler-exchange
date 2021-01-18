FILES=taler_error_codes.h \
  taler_error_codes.c \
  taler_error_codes.ts \
  taler_error_codes.py \
  taler_error_codes.kt

all: check $(FILES)

check: registry.rec
	recfix --check registry.rec

distclean:
	rm -f *.tmp

clean:
	rm -f $(FILES) *.tmp

prep:
	cd ../http-status-codes; make ; cd -

# Because prep is phony, this rule always triggers. To avoid
# combined.tmp cascading into triggering the other rules even
# if nothing changed, we use 'diff' to make sure that 'recsel'
# actually changed the result.
combined.tmp: registry.rec prep
	recsel -t TalerErrorCode -j HttpStatus -p Description,Name,Value,HttpStatus,HttpStatus.Value,HttpStatus.Identifier ../http-status-codes/registry.rec registry.rec > combined.xtmp
	if ! diff $@ combined.xtmp > /dev/null; then mv combined.xtmp $@; else rm combined.xtmp; fi

combined-escaped.tmp: combined.tmp
	sed 's/"/\\"/g' $^ > $@

length.tmp: combined.tmp
	recsel -p "Count(Description):ECS_LENGTH" combined.tmp > $@

taler_error_codes.h.tmp: combined.tmp h.template
	../format.sh h.template < combined.tmp > $@

taler_error_codes.h: h.header taler_error_codes.h.tmp h.footer
	cat $^ > $@

taler_error_codes.c.tmp: combined-escaped.tmp c.template
	../format.sh c.template < combined-escaped.tmp > $@

taler_error_codes-length.c.tmp: length.tmp c-length.template
	../format.sh c-length.template < length.tmp > $@

taler_error_codes.c: c.header taler_error_codes.c.tmp taler_error_codes-length.c.tmp c.footer
	cat $^ > $@

taler_error_codes.ts.tmp: combined.tmp ts.template
	../format.sh ts.template < combined.tmp > $@

taler_error_codes.ts: ts.header taler_error_codes.ts.tmp ts.footer
	cat $^ > $@

taler_error_codes.kt.tmp: combined.tmp kt.template
	../format.sh kt.template < combined.tmp > $@

taler_error_codes.kt: kt.header taler_error_codes.kt.tmp kt.footer
	cat $^ > $@

taler_error_codes.py.tmp: combined.tmp py.template
	../format.sh py.template < combined.tmp > $@

taler_error_codes.py: py.header taler_error_codes.py.tmp 
	cat $^ > $@


.PHONY: check clean distclean prep
