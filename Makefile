#
#	configurable file locations
#
# path to install the scponly binary:

# user configuration section end
C_FLAGS=-O2

all : scponly

clean : 
	rm -f *.o scponly *~

install: scponly 
	./install.sh

scponly:	scponly.c Makefile helper.c scponly.h 
	$(CC) ${C_FLAGS} -DWINSCP_COMPAT -o scponly scponly.c helper.c

scponly_no_compat:	scponly.c Makefile helper.c scponly.h
	$(CC) ${C_FLAGS} -o scponly scponly.c helper.c

love: clean all
