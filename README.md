Phpiredis
=========

Phpiredis wraps the [hiredis](https://github.com/redis/hiredis) library in a PHP extension to provide:

  - a very simple but efficient client library for Redis.
  - a fast incremental parser for the Redis protocol.

Installation
------------

Building and using phpiredis requires the `hiredis` library to be installed on your system. `hiredis`
is usually available in the repositories of recent versions of the most used linux distributions,
alternatively you can build it by yourself fetching the code from its
[canonical repository ](https://github.com/redis/hiredis).

```sh
git clone https://github.com/nrk/phpiredis.git
cd phpiredis
phpize && ./configure --enable-phpiredis
make && make install
```

If the configuration script cannot locate `hiredis` on your system, you can specify in which directory
it can be found using the `--with-hiredis-dir=` parameter (e.g. `--with-hiredis-dir=/usr/local`).

Phpiredis provides a basic test suite that can be launched with `make test`. Tests require a running
instance of `redis-server` listening on `127.0.0.1:6379`, but please remember that you should __never__
point to a Redis instance in production or holding data you are interested in because you could end up
losing everything stored on it.

If you notice a failing test or a bug, please do not hesitate to file a bug on the
[issue tracker](http://github.com/nrk/phpiredis/issues).

Usage
-----

Connecting to a redis server is as simple as calling the `phpiredis_connect()` function with a server
address as the first parameter and, optionally, a port number when the server is listening to a different
port than `6379` (the default one).

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

The `_bs` suffix of these functions indicates that they can handle binary key names or values as they
use the unified Redis protocol available since Redis >= 1.2.

Commands can still be sent using the old and deprecated inline protocol using `phpiredis_command()` and
`phpiredis_multi_command()` (not the lack of the `_bs` suffix) but it's highly discouraged and these
functions will most likely be removed in future versions of phpiredis:

```php
$response = phpiredis_command($redis, 'DEL test');

$response = phpiredis_multi_command($redis, array(
    'SET test 1',
    'GET test',
));
```

Contributing
------------
Contributions are extremely welcome! Just fork the project on GitHub, work on new features or bug fixes
using feature branches and [open a pull-request](http://github.com/nrk/phpiredis/issues) with a description
of the changes. If you are unsure about a proposal you can just open an issue to discuss it before writing
actual code.

Authors
-------
[Daniele Alessandri](https://github.com/nrk) (current maintainer)
[Sebastian Waisbrot](https://github.com/seppo0010) (original developer)

License
-------
The code for phpiredis is distributed under the terms of the BSD license (see [LICENSE](LICENSE)).
