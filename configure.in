dnl Process this file with autoconf to produce a configure script.

dnl stupid hack to properly parse $PATH for /bin
dnl if this breaks your configure, delete it
dnl all it does is append the first dir to the end
dnl first_part=`echo $PATH | cut -f1 -d\:`
dnl export PATH=$PATH:$first_part
PATH="$PATH:/usr/local/bin"

dnl
dnl Say what we are:
AC_INIT(scponly, 4.8, joe@sublimation.org)                                                                      

dnl AC_CONFIG_SRCDIR([helper.c])
AC_CONFIG_HEADER([config.h])

AC_CANONICAL_HOST

# this temp_os is a brutal hack until i look into the proper way to do this. ie- rtfm
temp_os=`echo $host_os | cut -c-7`
if test "$temp_os" = solaris; then
	AC_DEFINE(UNIX_COMPAT)
fi
temp_os=`echo $host_os | cut -c-4`
if test "$temp_os" = irix;  then
	AC_DEFINE(IRIX_COMPAT)
fi
temp_os=`echo $host_os | cut -c-3`
if test "$temp_os" = aix; then
	AC_DEFINE(UNIX_COMPAT)
fi

AC_DEFINE(HOST_OS,$host_os)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROG(PROG_CUT, cut)
AC_PATH_PROG(PROG_GREP, grep)
AC_PATH_PROG(PROG_SORT, sort)
AC_PATH_PROG(PROG_LDD, ldd)
AC_PATH_PROG(PROG_USERADD, useradd)
AC_PATH_PROG(PROG_CHOWN, chown)
AC_PATH_PROG(PROG_CHMOD, chmod)
AC_PATH_PROG(PROG_DIRNAME, dirname)
AC_PATH_PROG(PROG_ID, id)
AC_PATH_PROG(PROG_PW, pw)
AC_PATH_PROG(PROG_RM, rm)
AC_PATH_PROG(PROG_PWD_MKDB, pwd_mkdb)

dnl Features wanted for this installation:
dnl Command-line args to ./configure
dnl Call options "--disable-foo" if they default to on,
dnl call them "--enable-foo" otherwise.

AC_ARG_ENABLE([restrictive-names],
      AC_HELP_STRING([--disable-restrictive-names], [disable restrictive filename checks]),
      [
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([RESTRICTIVE_FILENAMES])
              fi
      ],[
dnl           Default clause. This one defaults to on.
              AC_DEFINE([RESTRICTIVE_FILENAMES])
      ])

AC_ARG_ENABLE([wildcards],
      AC_HELP_STRING([--disable-wildcards], [disable wildcard processing]),
      [
dnl           Did they say anything, including an explicit --enable-wildcards?
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([ENABLE_WILDCARDS])
              fi
      ],[
dnl           Default clause. This one defaults to on.
              AC_DEFINE([ENABLE_WILDCARDS])
      ])

AC_ARG_ENABLE([gftp-compat],
      AC_HELP_STRING([--disable-gftp-compat], [disable gftp compatibility]),
      [
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([GFTP_COMPAT])
              fi
      ],[
              AC_DEFINE([GFTP_COMPAT])
      ])

AC_ARG_WITH([sftp-server],
      AC_HELP_STRING([--with-sftp-server], [path to sftp-server binary [defaults to on+guessed]]),
      [
              if test "x$withval" != "xno"; then
dnl                   --without-sftp-server is a perfectly good
dnl                   way of requesting that sftp compatability *not*
dnl                   be present.
                      scponly_explicit_sftpserver_path=$withval
                      dnl  TODO: should test viability...
                      AC_DEFINE([ENABLE_SFTP])
                      scponly_sftp_compat=1
              fi
      ],[
              AC_DEFINE([ENABLE_SFTP])
              scponly_sftp_compat=1
      ])

AC_ARG_ENABLE([winscp-compat],
      AC_HELP_STRING([--enable-winscp-compat], [enable winscp (and scp) compatibility]),
      [
              if test "x$enableval" != "xno"; then
                      scponly_scp_compat=1
                      AC_DEFINE([ENABLE_SCP2])
                      AC_DEFINE([WINSCP_COMPAT])
              fi
      ],[
              echo -n      dnl  Defaults to off, must be turned on explicitly
      ])

AC_ARG_WITH([default-chdir],
	AC_HELP_STRING([--with-default-chdir=DIR], [cd to this directory after authentication (only for interactive logins)]),
		[
			AC_DEFINE([ENABLE_DEFAULT_CHDIR])
			AC_DEFINE_UNQUOTED([DEFAULT_CHDIR], ["$withval"]) 
		],[
			echo -n      dnl  Defaults to off, must be turned on explicitly
		])

AC_ARG_ENABLE([unison-compat],
      AC_HELP_STRING([--enable-unison-compat], [enable unison compatibility]),
      [
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([UNISON_COMPAT])
              fi
      ],[
              echo -n      dnl  Defaults to off, must be turned on explicitly
      ])
		
AC_ARG_ENABLE([scp-compat],
      AC_HELP_STRING([--enable-scp-compat], [enable scp compatibility]),
      [
              if test "x$enableval" != "xno"; then
					AC_DEFINE([ENABLE_SCP2])
					scponly_scp_compat=1
              fi
      ],[
              echo -n      dnl  Defaults to off, must be turned on explicitly
      ])

AC_ARG_ENABLE([rsync-compat],
      AC_HELP_STRING([--enable-rsync-compat], [enable rsync compatibility]),
      [
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([RSYNC_COMPAT])
              fi
      ],[
              echo -n      dnl  Defaults to off, must be turned on explicitly
      ])

AC_ARG_ENABLE([chrooted-binary],
      AC_HELP_STRING([--enable-chrooted-binary], [install chrooted binary 'scponlyc']),
      [
              if test "x$enableval" != "xno"; then
                      AC_SUBST([CHROOTED_NAME], [scponlyc])
                      AC_DEFINE([CHROOTED_NAME], ["scponlyc"])
              fi
      ],[
              echo -n
      ])

AC_ARG_ENABLE([chroot-checkdir],
      AC_HELP_STRING([--disable-chroot-checkdir], [disable checking chroot dir ownership]),
      [
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([CHROOT_CHECKDIR])
              fi
      ],[
dnl           Default clause. This one defaults to on.
              AC_DEFINE([CHROOT_CHECKDIR])
      ])

AC_ARG_ENABLE([svn-compat],
      AC_HELP_STRING([--enable-svn-compat], [enable subversion SCS cli compatibility]),
      [
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([SVN_COMPAT])
              fi
      ],[
              echo -n      dnl  Defaults to off, must be turned on explicitly
      ])

AC_ARG_ENABLE([svnserv-compat],
      AC_HELP_STRING([--enable-svnserv-compat], [enable subversion SCS svnserve compatibility]),
      [
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([SVNSERV_COMPAT])
              fi
      ],[
              echo -n      dnl  Defaults to off, must be turned on explicitly
      ])

AC_ARG_ENABLE([passwd-compat],
      AC_HELP_STRING([--enable-passwd-compat], [enable passwd compatibility]),
      [
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([PASSWD_COMPAT])
              fi
      ],[
              echo -n      dnl  Defaults to off, must be turned on explicitly
      ])

AC_ARG_ENABLE([quota-compat],
      AC_HELP_STRING([--enable-quota-compat], [enable quota compatibility]),
      [
              if test "x$enableval" != "xno"; then
                      AC_DEFINE([QUOTA_COMPAT])
              fi
      ],[
              echo -n      dnl  Defaults to off, must be turned on explicitly
      ])

if test "x$scponly_scp_compat" != "x"; then
	AC_MSG_NOTICE([enabling core WinSCP and Vanilla SCP binaries...])
	SCPONLY_PATH_PROG_DEFINE([PROG_SFTP_SERVER], [sftp-server],[/usr/lib:/usr/lib64:/usr/libexec:/usr/libexec/openssh:/usr/lib/ssh:/usr/lib64/ssh:/usr/local/libexec:/usr/lib/misc:/usr/lib/openssh])
	SCPONLY_PATH_PROG_DEFINE([PROG_LS],    [ls],    [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_SCP],   [scp],   [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_RM],    [rm],    [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_LN],    [ln],    [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_MV],    [mv],    [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_CHMOD], [chmod], [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_CHOWN], [chown], [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_CHGRP], [chgrp], [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_MKDIR], [mkdir], [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_RMDIR], [rmdir], [/bin:/usr/bin:/sbin:/usr/sbin])
fi

dnl Check for binaries required by the WinSCP compatibility mode
dnl winscp-compat conditionals:
if test "x$enable_winscp_compat" != "xno"; then
    AC_MSG_NOTICE([enabling WinSCP compatability...])
	SCPONLY_PATH_PROG_DEFINE([PROG_PWD],    [pwd],    [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_GROUPS], [groups], [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_ID], 	[id],     [/bin:/usr/bin:/sbin:/usr/sbin])
	SCPONLY_PATH_PROG_DEFINE([PROG_ECHO],   [echo],   [/bin:/usr/bin:/sbin:/usr/sbin])
fi

# unison-compat conditionals:
if test "x$enable_unison_compat" != "x" && test "x$enable_unison_compat" != "xno"; then
	AC_MSG_WARN([read the SECURITY document before enabling unison compatibility])
	AC_MSG_NOTICE([enabling unison compatability...])
	SCPONLY_PATH_PROG_DEFINE([PROG_UNISON],   [unison], [/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin])
fi	

# rsync-compat conditionals:
if test "x$enable_rsync_compat" != "x" && test "x$enable_rsync_compat" != "xno"; then
	AC_MSG_WARN([read the SECURITY document before enabling rsync compatibility])
	AC_MSG_NOTICE([enabling rsync compatability...])
	SCPONLY_PATH_PROG_DEFINE([PROG_RSYNC],   [rsync], [/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin])
fi	

dnl Find the svnserve and svn binaries
if test "x$enable_svnserv_compat" != "x" && test "x$enable_svnserv_compat" != "xno"; then
	AC_MSG_WARN([read the SECURITY document before enabling svnserv compatibility])
	AC_MSG_NOTICE([enabling svnserv compatibility...])
	SCPONLY_PATH_PROG_DEFINE([PROG_SVNSERV], [svnserve], [/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin])
fi

if test "x$enable_svn_compat" != "x" && test "x$enable_svn_compat" != "xno"; then
	AC_MSG_WARN([read the SECURITY document before enabling svn compatibility])
	AC_MSG_NOTICE([enabling subversion compatability...])
	SCPONLY_PATH_PROG_DEFINE([PROG_SVN],   [svn],   [/bin:/usr/bin:/usr/local/bin:/usr/local/bin])
fi

if test "x$enable_passwd_compat" != "x" && test "x$enable_passwd_compat" != "xno"; then
	AC_MSG_NOTICE([enabling passwd compatability...])
	SCPONLY_PATH_PROG_DEFINE([PROG_PASSWD],   [passwd],   [/bin:/usr/bin:/sbin:/usr/sbin])
fi

if test "x$enable_quota_compat" != "x" && test "x$enable_quota_compat" != "xno"; then
	AC_MSG_NOTICE([enabling quota compatability...])
	SCPONLY_PATH_PROG_DEFINE([PROG_QUOTA],   [quota],   [/bin:/usr/bin:/sbin:/usr/sbin])
fi

dnl with-sftp-server conditionals:
if test "x$scponly_sftp_compat" != "x"; then
  AC_MSG_NOTICE([enabling SFTP compatability...])
  if test "x$scponly_explicit_sftpserver_path" = "x"; then
      dnl Informed guess:
      SCPONLY_PATH_PROG_DEFINE([PROG_SFTP_SERVER], [sftp-server],
       [/usr/lib:/usr/lib/ssh:/usr/libexec/openssh:/usr/libexec:/usr/local/libexec])
      dnl Debian uses /usr/lib
      dnl Red Hat uses /usr/libexec/openssh
      dnl Many a *BSD uses $PATH itself (which is implicit + checked 1st)
      dnl Lupe says FreeBSD wants /usr/[local]/libexec
      dnl I've seen it in /usr/lib/ssh under Mandrake 8.1
  else
      dnl Do what SCPONLY_PATH_PROG_DEFINE does:
      echo checking for sftp-server... $scponly_explicit_sftpserver_path
      AC_DEFINE_UNQUOTED([PROG_SFTP_SERVER], ["$scponly_explicit_sftpserver_path"])
      scponly_PROG_SFTP_SERVER=$scponly_explicit_sftpserver_path
  fi
fi

dnl Checks for libraries.
dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h syslog.h unistd.h wordexp.h glob.h libgen.h getopt.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_CHECK_FUNCS([malloc atexit bzero strchr strerror glob wordexp strspn basename getopt])

AC_CHECK_DECL(optreset,
 AC_DEFINE([HAVE_OPTRESET],[1],[have optreset]),,
 [#include <getopt.h>] )

dnl AC_CONFIG_FILES([Makefile])
AC_OUTPUT([Makefile setup_chroot.sh])
