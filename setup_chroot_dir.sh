#!/bin/bash

BINARIES="ls sh scp sftp-server rm mkdir chmod rmdir pwd"

function usage {
	echo $0: usage:
	echo 	$0 \<directory to setup for chroot\>
	exit 1
}

function err {
	echo -e "\n\n\n$@\n"
	usage
}

function checkdir {
	if [ ! -d $1 ]; then 
		echo mkdir $1; 
	fi
}

function cond_copy {
	if [ "x$3" != "x" ]; then
		if [ ! -f $1$2 ]; then
			echo cp $3 $1$2
		fi
		return
	fi
	if [ ! -f $1/$2 ]; then
		echo cp $2 $1$2
	fi
}

if [ "x$1" = "x" ]; then
	usage
fi
TARGET=$1

echo "# Setting up a chrootable directory: $TARGET"
if [ ! -d $TARGET ]; then
	err $TARGET isnt a directory or does not exist!
	usage
fi

echo "# creating a bunch of directories, some of which probably aren't needed"
checkdir $1/usr
checkdir $1/usr/local
checkdir $1/usr/local/lib
checkdir $1/lib
checkdir $1/usr/lib
checkdir $1/usr/libexec
checkdir $1/bin
checkdir $TARGET/etc

# $CHBINDIR is for convenience, if you change it, this
# script breaks.  this is lame.
CHBINDIR=/bin/
echo "# copying neccesary binaries in"
for bin in $BINARIES; do
	destbinpath=$CHBINDIR$bin
	srcbinpath=`which $bin | grep -v ^alias`
	fullpath_list="$srcbinpath $fullpath_list"
	if [ ! -f $TARGET/$destbinpath ]; then
		cond_copy $TARGET $destbinpath $srcbinpath
	fi
done
echo "# copying libraries in"
LIB_LIST=`ldd $fullpath_list 2> /dev/null | cut -f2 -d\> | cut -f1 -d\( | grep "^ " | sort -u`
LIB_LIST="$LIB_LIST /usr/libexec/ld.so"
if [ "x$LIB_LIST" != "x" ]; then
	for lib in $LIB_LIST; do
		if [ ! -f $TARGET/$lib ]; then
			cond_copy $TARGET $lib
		fi
	done
fi

echo "# copying passwd database and group file in"
cond_copy $TARGET /etc/group
cond_copy $TARGET /etc/pwd.db
cond_copy $TARGET /etc/passwd

exit 0



