PHP_ARG_ENABLE(phpiredis, whether to enable phpiredis support,
[ --enable-phpiredis   Enable phpiredis support])

PHP_ARG_WITH(hiredis-dir, for hiredis library,
[  --with-hiredis-dir[=DIR]   Set the path to hiredis install prefix.], yes)

if test "$PHP_PHPIREDIS" = "yes"; then
  AC_DEFINE(HAVE_PHPIREDIS, 1, [Whether you have phpiredis])

  PKG_CONFIG=`which pkg-config`

  if test "$PHP_HIREDIS_DIR" != "no" && test "$PHP_HIREDIS_DIR" != "yes"; then
    for i in $PHP_HIREDIS_DIR /usr /usr/local; do
      if test -r $i/include/$SEARCH_FOR; then
        HIREDIS_DIR=$i
       break
      fi
     done
     if test -z $HIREDIS_DIR; then
       AC_MSG_RESULT(not found)
       AC_MSG_ERROR(Could not find hiredis in search pathts)
     fi
     AC_MSG_RESULT(Found hiredis in $HIREDIS_DIR)
     PHP_EVAL_LIBLINE(-lhiredis)
     PHP_EVAL_INCLINE(-I$HIREDIS_DIR/include)
  elif $PKG_CONFIG --exists hiredis; then
    HIREDIS_VERSION=`$PKG_CONFIG --modversion hiredis`
    AC_MSG_RESULT([Found hiredis $HIREDIS_VERSION])
    PHP_EVAL_INCLINE(`$PKG_CONFIG --cflags-only-I hiredis`)
    PHP_EVAL_LIBLINE(`$PKG_CONFIG --libs hiredis`, PHPIREDIS_SHARED_LIBADD)
  else
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Ooops ! hiredis not found)
  fi

  PHP_SUBST(PHPIREDIS_SHARED_LIBADD)
  PHP_NEW_EXTENSION(phpiredis, phpiredis.c, $ext_shared)
fi
