#
#	configurable file locations
#
# path to install the scponly binary:

# user configuration section end

all : scponly

clean : 
	rm -f *.o scponly

install: scponly 
	./install.sh
	
scponly:	scponly.c Makefile helper.c
	$(CC) -g -o scponly scponly.c helper.c

love: clean all
