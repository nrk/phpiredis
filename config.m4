PHP_ARG_ENABLE(hello, whether to enable Hello World support,
[ --enable-hello   Enable Hello World support])

if test "$PHP_HELLO" = "yes"; then
  AC_DEFINE(HAVE_HELLO, 1, [Whether you have Hello World])

  extra_sources="lib/hiredis/hiredis.c lib/hiredis/sds.c lib/hiredis/net.c"
  PHP_NEW_EXTENSION(hello, hello.c $extra_sources, $ext_shared)
  PHP_ADD_BUILD_DIR(lib/hiredis)
fi
