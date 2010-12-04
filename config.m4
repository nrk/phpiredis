PHP_ARG_ENABLE(phpiredis, whether to enable phpiredis support,
[ --enable-phpiredis   Enable phpiredis support])

if test "$PHP_PHPIREDIS" = "yes"; then
  AC_DEFINE(HAVE_PHPIREDIS, 1, [Whether you have phpiredis])

  extra_sources="lib/hiredis/hiredis.c lib/hiredis/sds.c lib/hiredis/net.c"
  PHP_NEW_EXTENSION(phpiredis, phpiredis.c $extra_sources, $ext_shared)
  PHP_ADD_BUILD_DIR(lib/hiredis)
fi
