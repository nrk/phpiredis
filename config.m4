PHP_ARG_ENABLE(phpiredis, whether to enable phpiredis support,
[ --enable-phpiredis   Enable phpiredis support])

PHP_ARG_WITH(hiredis-dir, for hiredis library,
[  --with-hiredis-dir[=DIR]   Set the path to hiredis install prefix.], yes)

if test "$PHP_PHPIREDIS" = "yes"; then

  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  if test "x$PKG_CONFIG" = "xno"; then
    AC_MSG_RESULT([pkg-config not found])
    AC_MSG_ERROR([Please reinstall the pkg-config distribution])
  fi

  if test "$PHP_HIREDIS_DIR" != "no" && test "$PHP_HIREDIS_DIR" != "yes"; then
    for i in $PHP_HIREDIS_DIR /usr /usr/local; do
      if test -r $i/include/hiredis/hiredis.h; then
        HIREDIS_DIR=$i
       break
      fi
     done
     if test -z $HIREDIS_DIR; then
       AC_MSG_RESULT(not found)
       AC_MSG_ERROR(Could not find hiredis in search paths)
     fi

     PHP_ADD_LIBRARY_WITH_PATH(hiredis, [$HIREDIS_DIR/$PHP_LIBDIR], PHPIREDIS_SHARED_LIBADD)
     PHP_ADD_INCLUDE([$HIREDIS_DIR/lib])

  #
  # Why is this using pkg-config by default? hiredis doesn't seem to install pkgconfig files
  #
  elif $PKG_CONFIG --exists hiredis; then
    HIREDIS_VERSION=`$PKG_CONFIG --modversion hiredis`
    AC_MSG_RESULT([Found hiredis $HIREDIS_VERSION])
    PHP_EVAL_INCLINE(`$PKG_CONFIG --cflags-only-I hiredis`)
    PHP_EVAL_LIBLINE(`$PKG_CONFIG --libs hiredis`, PHPIREDIS_SHARED_LIBADD)
  else
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Ooops ! hiredis not found)
  fi

  AC_DEFINE(HAVE_PHPIREDIS, 1, [Whether you have phpiredis])
  PHP_SUBST(PHPIREDIS_SHARED_LIBADD)
  PHP_NEW_EXTENSION(phpiredis, phpiredis.c, $ext_shared)
fi
