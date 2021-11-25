
AC_PREREQ([2.64])
AC_INIT([micro-manager],[1.0])
AC_CONFIG_MACRO_DIR([./mmCoreAndDevices/m4])
AC_CONFIG_SRCDIR([mmCoreAndDevices/MMDevice/MMDevice.cpp])
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([foreign 1.11])
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

AC_PROG_CXX([clang++ llvm-g++ g++ c++])
AC_PROG_CC([clang llvm-gcc gcc cc])


# Find Micro-Manager headers
micromanager_cpp_path=${ac_pwd}/mmCoreAndDevices
micromanager_path=${micromanager_cpp_path}/..
MMDEVAPI_CXXFLAGS="-I${micromanager_cpp_path}/MMDevice ${BOOST_CPPFLAGS}"
AC_SUBST(MMDEVAPI_CXXFLAGS)

# Find Micro-Manager static device library
MMDEVAPI_LIBADD="${micromanager_cpp_path}/MMDevice/libMMDevice.la"
AC_SUBST(MMDEVAPI_LIBADD)

# Apply appropriate libtool options for the Micro-Manager device API
MMDEVAPI_LDFLAGS="-module -avoid-version -shrext \"\$(MMSUFFIX)\""
AC_SUBST(MMDEVAPI_LDFLAGS)

MM_INSTALL_DIRS


# Micro-Manager libraries have a prefix & suffix to make them unique.  These are defined here:
case $host in
   *-*-linux*)
   MMSUFFIX=".so.0"
   MMPREFIX="libmmgr_dal_"
   ;;
esac
if test -z "$MMSUFFIX"; then
  MMSUFFIX=""
fi
if test -z "$MMPREFIX"; then
  MMPREFIX="mmgr_dal_"
fi

AC_MSG_CHECKING(library suffix)
AC_MSG_RESULT($MMSUFFIX)
AC_SUBST(MMSUFFIX)
AC_MSG_CHECKING(library prefix)
AC_MSG_RESULT($MMPREFIX)
AC_SUBST(MMPREFIX)


# Checks for library functions.
m4_warn([obsolete],
[The preprocessor macro `STDC_HEADERS' is obsolete.
  Except in unusual embedded environments, you can safely include all
  ISO C90 headers unconditionally.])dnl
# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_PROG_EGREP

AC_CHECK_FUNCS([memset])


# Install Device Adapter API library and headers
install_mmdevapi=false
#AC_ARG_ENABLE(inst-devapi,
#    [  --enable-inst-devapi    Install the Device Adapter API library and headers ],
#    [ install_mmdevapi=true ])
AM_CONDITIONAL([INSTALL_MMDEVAPI], [test x$install_mmdevapi = xtrue])


# This is the list of subdirectories containing a Makefile.am.
m4_define([device_adapter_dirs], [m4_strip([
  mmCoreAndDevices/MMDevice
  mmCoreAndDevices/MMDevice/unittest
  ArdIllu
  ArduControl
])])

AC_CONFIG_FILES(Makefile m4_map_args_w(device_adapter_dirs, [], [/Makefile], [ ]))

AC_OUTPUT
