#
#	configurable file locations
#
# path to install the scponly binary:

# NOTE: if you change these paths, you must also change them in scponly.h
#	this will obviously demand a recompile if you have already compiled

INSTALL_PATH=/usr/local/sbin

# pathname of your logfile
LOG_FILE=/var/log/scponly.log

# user configuration section end

all : scponly

clean : 
	rm -f *.o scponly

install: scponly
	@echo
	@echo INSTALLING scponly in $(INSTALL_PATH)
	cp scponly $(INSTALL_PATH)
	@echo
	@echo CREATING LOGFILE as $(LOG_FILE)
	echo -n "scponly installed at " >> $(LOG_FILE)
	date >> $(LOG_FILE)
	chmod 666 $(LOG_FILE)

scponly:
	$(CC) -o scponly scponly.c helper.c
scponly_chroot:
	$(CC) -DCHROOT -o scponly scponly.c helper.c

love: clean all
