#
#	configurable file locations
#
# path to install the scponly binary:

CFLAGS += -O2
PREFIX ?= /usr/local
INSTALLPATH = $(PREFIX)/sbin

# user configuration section end

CHROOTED_NAME != grep CHROOTED_NAME scponly.h | cut -f2 -d\"
SHELLS = /etc/shells

all : scponly

clean : 
	rm -f *.o scponly *~

scponly:	scponly.c Makefile helper.c scponly.h 
	$(CC) ${CFLAGS} -DWINSCP_COMPAT -o scponly scponly.c helper.c

scponly_no_compat:	scponly.c Makefile helper.c scponly.h
	$(CC) ${CFLAGS} -o scponly scponly.c helper.c

install: scponly 
	@if [ `id -u` != "0" ]; then \
		echo "You must be root to install scponly!\n"; \
		exit 1; \
	fi
	cp -f scponly $(INSTALLPATH)/scponly
	@if [ `grep -c scponly$$ $(SHELLS)` -eq 0 ]; then \
		echo $(INSTALLPATH)/scponly >> $(SHELLS) ; \
		echo "Appended" $(INSTALLPATH)/scponly "to" $(SHELLS) ; \
	fi
	cp -f scponly.8 $(PREFIX)/man/man8/scponly.8
	chmod a+rx $(INSTALLPATH)/scponly
	chmod a+r $(PREFIX)/man/man8/scponly.8

install-chroot: install
	ln -fs $(INSTALLPATH)/scponly $(INSTALLPATH)/$(CHROOTED_NAME)
	chmod u+s $(INSTALLPATH)/scponly
	@if [ `grep -c $(CHROOTED_NAME)$$ $(SHELLS)` -eq 0 ]; then \
	   echo $(INSTALLPATH)/$(CHROOTED_NAME) >> $(SHELLS); \
	   echo "Appended" $(INSTALLPATH)/$(CHROOTED_NAME) "to" $(SHELLS) ; \
	fi

jail:
	./setup_chroot.sh

love: clean all
