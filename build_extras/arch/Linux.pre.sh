#
#       this is a presetup script for Linux
#
#   any custom modifications to setup_chroot.sh variables could occur here
#
#	for specific distros of linux, use uname and if/then clauses...
#	perhaps sourcing other scripts, send changes to joe@sublimation.org
#
#
# update the real ld.so.cache, and include it and the config to be copied.
# could use ldconfig -r, but this is probably more portable.
#

ldconfig

LIB_LIST="$LIB_LIST /etc/ld.so.cache  /etc/ld.so.conf"
