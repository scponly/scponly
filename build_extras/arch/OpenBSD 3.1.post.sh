#
#	this is a sample postsetup script for OpenBSD 3.1
#
#	an example of specific commands to use INSTEAD of the best guess
#	in setup_chroot.sh
#

grep $targetuser /etc/master.passwd > $targetdir/etc/master.passwd
pwd_mkdb -d "$targetdir/etc" $targetdir/etc/master.passwd
rm -rf $targetdir/etc/master.passwd $targetdir/etc/spwd.db
