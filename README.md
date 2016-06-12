# Phpiredis #

[![Software license][ico-license]](LICENSE)
[![Build status][ico-travis]][link-travis]

Phpiredis is an extension for PHP 5.x and 7.x based on [hiredis](https://github.com/redis/hiredis)
that provides a simple and efficient client for Redis and a fast incremental parser / serializer for
the [RESP protocol](http://redis.io/topics/protocol).

## Installation ##

Building and using this extension requires `hiredis` (>=0.9.0 <1.0.0) to be installed on the system.
`hiredis` is usually available in the repositories of most Linux distributions, alternatively it is
possible to build it by fetching the code from its [repository](https://github.com/redis/hiredis).

```sh
git clone https://github.com/nrk/phpiredis.git
cd phpiredis
phpize && ./configure --enable-phpiredis
make && make install
```

When the configuration script is unable to locate `hiredis` on your system, you can specify in which
directory it can be found using `--with-hiredis-dir=` (e.g. `--with-hiredis-dir=/usr/local`).

Phpiredis provides a basic test suite that can be launched with `make test`. Tests require a running
instance of `redis-server` listening on `127.0.0.1:6379` but __make sure__ that your server does not
hold data you are interested: you could end up losing everything stored on it!

If you notice a failing test or a bug, you can contribute by opening a pull request on GitHub or
simply file a bug on our [issue tracker](http://github.com/nrk/phpiredis/issues).

## Usage ##

Connecting to Redis is as simple as calling the `phpiredis_connect()` function with a server address
as the first parameter and an optional port number when the server is listening to a different port
than the default `6379`:

```php
$redis = phpiredis_connect('127.0.0.1', 6379);      // normal connection
$redis = phpiredis_pconnect('127.0.0.1', 6379);     // persistent connection
```

Alternatively you can connect to redis using UNIX domain socket connections.

```php
$redis = phpiredis_connect('/tmp/redis.sock');      // normal connection
$redis = phpiredis_pconnect('/tmp/redis.sock');     // persistent connection
```

Once the connection is established, you can send commands to Redis using `phpiredis_command_bs()` or
pipeline them using `phpiredis_multi_command_bs()`:

```php
$response = phpiredis_command_bs($redis, array('DEL', 'test'));

$response = phpiredis_multi_command_bs($redis, array(
    array('SET', 'test', '1'),
    array('GET', 'test'),
));
```

The `_bs` suffix indicates that these functions can handle binary key names or values by using the
unified Redis protocol available since Redis >= 1.2.

Commands can still be sent using the old and deprecated inline protocol using `phpiredis_command()`
and `phpiredis_multi_command()` (note the lack of the `_bs` suffix) but it's highly discouraged and
these functions will be removed in future versions of phpiredis.

```php
$response = phpiredis_command($redis, 'DEL test');

$response = phpiredis_multi_command($redis, array(
    'SET test 1',
    'GET test',
));
```

## Contributing ##

Any kind of contribution is extremely welcome! Just fork the project on GitHub, work on new features
or bug fixes using feature branches and [open pull-requests](http://github.com/nrk/phpiredis/issues)
with concise but complete descriptions of your changes. If you are unsure about a proposal, you can
just open an issue to discuss it before writing actual code.

## Authors ##

[Daniele Alessandri](https://github.com/nrk) (current maintainer)
[Sebastian Waisbrot](https://github.com/seppo0010) (original developer)

## License ##

The code for phpiredis is distributed under the terms of the BSD license (see [LICENSE](LICENSE)).

[ico-license]: https://img.shields.io/github/license/nrk/phpiredis.svg?style=flat-square
[ico-travis]: https://img.shields.io/travis/nrk/phpiredis.svg?style=flat-square

[link-travis]: https://travis-ci.org/nrk/phpiredis
