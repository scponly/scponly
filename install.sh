#!/bin/sh

function fail {
	echo -e $@
	exit 1
}

function getwd {
	query="$1"
	default="$2"
	echo -en "$query [$default]" | cat >&2
	read response
	if [ x$response = "x" ]; then
		response=$default
	fi
	echo $response
}

function getyn {
	query="$@"
	echo -en $query | cat >&2
	read response
	while [ x$response != "xy" -a x$response != "xn" ]; do
		echo -e "\n'y' or 'n' only please...\n" | cat >&2
		echo -en $query | cat >&2
		read response
	done	
	echo $response
}

echo -e "\n\n\n\nscponly interactive installer\n\n"
echo -e "This script is only to be run from the same PWD as the"
echo -e "main source tree for scponly.\n\n"

INSTALL_PATH=`grep INSTALL_PATH scponly.h | cut -f2 -d\"`
CHROOTED_NAME=`grep CHROOTED_NAME scponly.h | cut -f2 -d\"`
OLD_LOG_FILE=/var/log/scponly.log

if [ `id -u` != "0" ]; then
	fail "you must be root to run this script\n"
fi

installpath=`getwd 'Where do you want to install scponly?' '/usr/local/sbin'`
echo
chroot=`getyn 'Would you like to install chroot() supported scponly? (y/n) '`

# nonchroot install here

cp -f scponly $installpath
if test -f $OLD_LOG_FILE; then
    chown 0.0 $OLD_LOG_FILE
    chmod 0400 $OLD_LOG_FILE
fi
bin_already=`grep scponly$ /etc/shells | wc -l | awk -- '{print $1}'`
if [ $bin_already -eq 0 ]; then
	echo $installpath/scponly >> /etc/shells
fi

if [ $chroot = 'n' ]; then
	exit 0
fi

# everything below is chroot specific

ln -fs $installpath/scponly $installpath/$CHROOTED_NAME
chmod u+s $installpath/scponly 
bin_already=`grep ${CHROOTED_NAME}$ /etc/shells | wc -l | awk -- '{print $1}'`
if [ $bin_already -eq 0 ]; then
	echo $installpath/$CHROOTED_NAME >> /etc/shells
fi

makejail=`getyn 'Would you like to configure a scponly chrooted user now? '`

if [ $makejail = 'y' ]; then
	./setup_chroot.sh
fi

exit 0


