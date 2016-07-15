PHP_ARG_ENABLE(phpiredis, whether to enable phpiredis support,
[ --enable-phpiredis   Enable phpiredis support])

PHP_ARG_WITH(hiredis-dir, for hiredis library,
[  --with-hiredis-dir[=DIR]   Set the path to hiredis install prefix.], yes)

if test "$PHP_PHPIREDIS" = "yes"; then


  AC_MSG_CHECKING([for hiredis installation])

  #
  # Caller wants to check this path specifically
  #
  if test "x$PHP_HIREDIS_DIR" != "xno" && test "x$PHP_HIREDIS_DIR" != "xyes"; then
    if test -r "$PHP_HIREDIS_DIR/include/hiredis/hiredis.h"; then
      HIREDIS_DIR=$PHP_HIREDIS_DIR
      break
    fi
  else
    for i in /usr/local /usr /opt /opt/local; do
      if test -r "$i/include/hiredis/hiredis.h"; then
        HIREDIS_DIR=$i
        break
      fi
    done
  fi

  if test "x$HIREDIS_DIR" = "x"; then
    AC_MSG_ERROR([not found])
  fi

  AC_MSG_RESULT([found in $HIREDIS_DIR])

  PHP_ADD_LIBRARY_WITH_PATH(hiredis, [$HIREDIS_DIR/$PHP_LIBDIR], PHPIREDIS_SHARED_LIBADD)
  PHP_ADD_INCLUDE([$HIREDIS_DIR/include])

  AC_DEFINE(HAVE_PHPIREDIS, 1, [Whether you have phpiredis])
  PHP_SUBST(PHPIREDIS_SHARED_LIBADD)
  PHP_NEW_EXTENSION(phpiredis, phpiredis.c crc16.c, $ext_shared)
fi
