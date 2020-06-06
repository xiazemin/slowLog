dnl $Id$
dnl config.m4 for extension slowLog

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

 PHP_ARG_WITH(slowLog, for slowLog support,
 Make sure that the comment is aligned:
 [  --with-slowLog             Include slowLog support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(slowLog, whether to enable slowLog support,
dnl Make sure that the comment is aligned:
dnl [  --enable-slowLog           Enable slowLog support])

if test "$PHP_SLOWLOG" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-slowLog -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/slowLog.h"  # you most likely want to change this
  dnl if test -r $PHP_SLOWLOG/$SEARCH_FOR; then # path given as parameter
  dnl   SLOWLOG_DIR=$PHP_SLOWLOG
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for slowLog files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       SLOWLOG_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$SLOWLOG_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the slowLog distribution])
  dnl fi

  dnl # --with-slowLog -> add include path
  dnl PHP_ADD_INCLUDE($SLOWLOG_DIR/include)

  dnl # --with-slowLog -> check for lib and symbol presence
  dnl LIBNAME=slowLog # you may want to change this
  dnl LIBSYMBOL=slowLog # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $SLOWLOG_DIR/$PHP_LIBDIR, SLOWLOG_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_SLOWLOGLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong slowLog lib version or lib not found])
  dnl ],[
  dnl   -L$SLOWLOG_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(SLOWLOG_SHARED_LIBADD)

  PHP_NEW_EXTENSION(slowLog, slowLog.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
