#
# Makefile.am for snapper/scripts
#

if HAVE_PAM

pam_snapperdir = /usr/lib/pam_snapper

pam_snapper_SCRIPTS =			\
	pam_snapper_homeconvert.sh	\
	pam_snapper_pamconfig.sh	\
	pam_snapper_useradd.sh		\
	pam_snapper_userdel.sh

endif

EXTRA_DIST = snapper-hourly snapper-daily zypp-plugin.py $(pam_snapper_SCRIPTS)

install-data-local:
if HAVE_ZYPP
	install -D zypp-plugin.py $(DESTDIR)/usr/lib/zypp/plugins/commit/snapper.py
endif

