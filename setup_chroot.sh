#!/bin/sh

# the following is a list of binaries that will be staged in the target dir
BINARIES="ls sh scp sftp-server rm mkdir chmod rmdir pwd mv chown ln groups"

# this line will need to be changed if you've also changed
# the CHROOTED_NAME macro in scponly.h

scponlybin=`which scponlyc`

# a function to display a failure message and then exit 
function fail {
	echo -e $@
	exit 1
}

# "get with default" function
# this function prompts the user with a query and default reply
# it returns the user reply
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

# "get yes no" function
# this function prompts the user with a query and will continue to do so
# until they reply with either "y" or "n"
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

# we need to be root
if [ `id -u` != "0" ]; then
	fail "you must be root to run this script\n"
fi

# makes sure a directory is set up correctly
function checkdir {
	if [ ! -d $1 ]; then 
		mkdir $1; 
	fi
	if [ ! -d $1 ]; then 
		fail "couldn't create dir $1"
	fi
}

# copies a file if it isnt already at the destination location
function cond_copy {
	if [ "x$3" != "x" ]; then
		if [ ! -f $1$2 ]; then
			cp $3 $1$2
		fi
		return
	fi
	if [ ! -f $1/$2 ]; then
		cp $2 $1$2
	fi
}

echo -e "\n\nscponly chroot installer\n"
echo -n "Install for chroot to what directory? "
read targetdir
if [ "x$targetdir" = "x" ]; then
	fail "need to specify a target directory"
fi

echo -n "Install for what username? "
read targetuser
if [ "x$targetuser" = "x" ]; then
	fail "need to specify a username"
fi

echo -e "\nSetting up a chrootable directory $targetdir for $targetuser\n"
if [ ! -d $targetdir ]; then
	fail $targetdir isnt a directory or does not exist!
fi

echo -e "\ncreating a bunch of directories, some of which probably aren't needed\n"
checkdir $targetdir/usr
checkdir $targetdir/usr/local
checkdir $targetdir/usr/local/lib
checkdir $targetdir/lib
checkdir $targetdir/usr/lib
checkdir $targetdir/usr/libexec
checkdir $targetdir/bin
checkdir $targetdir/etc

# $CHBINDIR is for convenience, if you change it, this
# script breaks.  this is lame.
CHBINDIR=/bin/
echo "# copying neccesary binaries in"
for bin in $BINARIES; do
	destbinpath=$CHBINDIR$bin
	srcbinpath=`which $bin | grep -v ^alias`
	fullpath_list="$srcbinpath $fullpath_list"
	if [ ! -f $targetdir/$destbinpath ]; then
		cond_copy $targetdir $destbinpath $srcbinpath
	fi
done
echo -e "\ncopying libraries in"
LIB_LIST=`ldd $fullpath_list 2> /dev/null | cut -f2 -d\> | cut -f1 -d\( | grep "^ " | sort -u`
LIB_LIST="$LIB_LIST /usr/libexec/ld.so"
if [ "x$LIB_LIST" != "x" ]; then
	for lib in $LIB_LIST; do
		if [ ! -f $targetdir/$lib ]; then
			cond_copy $targetdir $lib
		fi
	done
fi

echo -e "\nsetting up chroot dir for user $targetuser\n"

useradd -d "$targetdir" -s "$scponlybin" $targetuser
if [ $? -ne 0 ]; then
	fail "if this user exists, remove it and try again"
fi

# the following is VERY BSD centric
# i check for pwd_mkdb before trying to use it
PWD_MKDB=`which pwd_mkdb`
if [ x$PWD_MKDB = "x" ]; then
	echo this script requires pwd_mkdb to stage a partial password database in your new chrooted dir
	echo depending on your UNIX flavor, the following may be a sufficient substitue:
	echo -e "\n"grep $targetuser /etc/passwd > $targetdir/etc/passwd"
	fail "please email joe@sublimation.org with the steps you used if you succeed on a platform this script will not run on and i will update the script"
fi
grep $targetuser /etc/master.passwd > $targetdir/etc/master.passwd
pwd_mkdb -d "$targetdir/etc" $targetdir/etc/master.passwd
rm -rf $targetdir/etc/master.passwd $targetdir/etc/spwd.db

exit 0

