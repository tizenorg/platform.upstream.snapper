#
# Makefile.am for snapper/scripts
#

EXTRA_DIST = snapper-hourly snapper-daily zypp-plugin.py

install-data-local:
if HAVE_ZYPP
	install -D zypp-plugin.py $(DESTDIR)/usr/lib/zypp/plugins/commit/snapper.py
endif

