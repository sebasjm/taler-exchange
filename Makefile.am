# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

if DOC_ONLY
if ENABLE_DOC
  SUBDIRS = . contrib doc
else
  SUBDIRS = . contrib
endif
else
if ENABLE_DOC
  SUBDIRS = . contrib src doc
else
  SUBDIRS = . contrib src
endif
endif

BUILT_SOURCES = src/include/taler_error_codes.h

src/include/taler_error_codes.h: contrib/gana/gnu-taler-error-codes/registry.rec
	contrib/gana-update.sh

@DX_RULES@

ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST =  \
  AUTHORS
