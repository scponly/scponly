dnl Defines $1 in config.h to the full path to the binary $2, with
dnl assertions. $3 contains an optional colon-separated list of
dnl directories besides $PATH to search.
AC_DEFUN([SCPONLY_PATH_PROG_DEFINE],
	 [AC_PATH_PROG([scponly_$1], [$2], [],
                       [`echo "$PATH:$3" | sed -e 's/:/ /'`])
	  test -z $scponly_$1 && echo "Can't find path to '$2'" && exit 1
	  AC_DEFINE_UNQUOTED([$1], "$scponly_$1")])


