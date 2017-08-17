#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([v120],[1.0],[pbailey@highlandtechnology.com])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([v120_tui/rc/V545.RNM])
AC_CONFIG_HEADERS([config.h])

LT_PREREQ([2.2])
LT_INIT([dlopen])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AM_PROG_CC_C_O

dnl Unless you know what you're doing, keep this always "no".
dnl It's meant to preclude the use of certain Linux-only system
dnl headers, so the source code can be developed on a non-Linux-
dnl but-otherwise-*nix computer.  If you set it to "yes", it's
dnl a good idea to also rename the package in AM_INIT above to
dnl something like v120-debug.
dnl
dnl Never commit to version control with this set as "yes"
v120_nolinux_debug=no

if test "x${v120_nolinux_debug}" = "xyes"; then
 AC_DEFINE([V120_NOLINUX_DEBUG],[1],
           [If 1, then this is NOT a valid release build!])
fi

# Fail build if libs, v120 cli, and v120irqd cannot be built.
# These are $fail_headers and $fail_libs.
fail_headers='assert ctype errno fcntl getopt poll signal stdarg stdbool
 stddef stdint stdio stdlib string syslog sys/mman sys/ioctl sys/socket
 sys/types unistd'
for i in $fail_headers; do
  AC_CHECK_HEADER([${i}.h], ,[AC_MSG_ERROR([${i}.h missing])])
done

# Can we build a true driver?
fail_headers='asm/ioctl linux/types'
if test "x${v120_nolinux_debug}" = "xno"; then
  for i in $fail_headers; do
    AC_CHECK_HEADER([${i}.h], ,[AC_MSG_ERROR([${i}.h missing])])
  done
fi

# Example dir headers (not counting ones we already checked above): If we
# don't have these, do not try to build examples, but don't fail
# configure over it.
local_have_examples=yes
for i in sched sys/stat sys/wait argp; do
  AC_CHECK_HEADER([${i}.h], ,[local_have_examples=no])
done
# Examples use argp_parse(), instead of getop[_long](), which is used
# everywhere else.  It adds just one more thing that could break
# configuration, but I'm not going to change the C code for the
# non-essential examples over that.
AC_CHECK_FUNC([argp_parse], ,[local_have_examples=no])

# pthread is optional for v120irqd. AC_CHECK_LIB covers the config.h
# define for this.  pthread is not optional for examples/, but since
# examples/ are optional, don't throw an error if pthread is missing.
have_pthread=yes
AC_CHECK_LIB(pthread, pthread_mutex_lock, ,have_pthread=no)
if test "x${have_pthread}" = "xyes"; then
  AC_CHECK_HEADER(pthread.h, ,have_pthread=no)
fi
if test "x${have_pthread}" = "xno"; then
  AC_MSG_WARN([pthread missing])
  local_have_examples=no
fi
AM_CONDITIONAL([BUILD_EXAMPLES], [test "x${local_have_examples}" = "xyes"])

# TODO: Robustulate this
BASH_COMPLETION_DIR=`pkg-config --variable=completiondir bash-completion \
     2>/dev/null`
if test -z "${completiondir}"; then
    BASH_COMPLETION_DIR="$datadir/bash-completion/completions"
fi
AC_SUBST([BASH_COMPLETION_DIR])
echo "Bash completions sent to ${BASH_COMPLETION_DIR}"

# Check if we can compile v120_tui, with ncurses if possible.
local_have_v120_tui=yes
local_have_ncurses=yes
AC_CHECK_LIB(ncurses, getch, ,local_have_ncurses=no)
if test "x${local_have_ncurses}" = "xyes"; then
  AC_CHECK_HEADER(ncurses.h, , local_have_ncurses=no)
fi
if test "x${local_have_ncurses}" = "xno"; then
  AC_MSG_WARN([ncurses missing v120_tui will be non-optimal])
  AC_CHECK_HEADER(termios.h, , local_have_v120_tui=no)
  AC_CHECK_FUNC(isatty, , local_have_v120_tui=no)
  AC_CHECK_FUNC(tcgetattr, , local_have_v120_tui=no)
  AC_CHECK_FUNC(tcsetattr, , local_have_v120_tui=no)
  AC_CHECK_FUNC(select, , local_have_v120_tui=no)
fi
AC_CHECK_FUNC(getenv, , local_have_v120_tui=no)
AM_CONDITIONAL([BUILD_V120_TUI], [test "x${local_have_v120_tui}" = "xyes"])

# TODO: Check these additional headers for v120_tui:

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([strtoul strtoull])
AC_CHECK_FUNC(ilog2f)

AC_CONFIG_FILES([Makefile
                 libV120irqd/Makefile
                 libV120/Makefile
                 include/Makefile
                 v120/Makefile
                 v120irqd/Makefile
                 test/Makefile
                 man/Makefile
                 examples/Makefile
                 v120_tui/Makefile
                 v120_tui/rc/Makefile])

# ==== Print final messages to user =====================================

AC_OUTPUT

echo \
"-------------------------------------------------

${PACKAGE_NAME} Version ${PACKAGE_VERSION}

Prefix: '${prefix}'.
Compiler: '${CC} ${CFLAGS} ${CPPFLAGS}'

Able to build libs?       yes
Able to build v120 (cli)? yes
Able to build v120irqd?   yes
     ...with pthread?     ${have_pthread}
Able to build examples?   ${local_have_examples}
Able to build v120_tui?   ${local_have_v120_tui}
     ...with ncurses?     ${local_have_ncurses}
"
if test "x${v120_nolinux_debug}" = "xyes"; then
  echo "\
    ***************************************
    *          !!! WARNING !!!            *
    *                                     *
    * This build has WRONG IOCTL NUMBERS. *
    *                ^^^^^^^^^^^^^^^^^^^  *
    * It is for debug purposes only.  Set *
    * \$v120_nolinux_debug in configure.ac *
    * to \"no\", run autoreconf, then try   *
    * this configure script again.        *
    ***************************************
"
fi
echo "\
Now type 'make @<:@<target>@:>@'
    where the optional <target> is:
        all         - build all binaries
        install     - install everything

--------------------------------------------------"
