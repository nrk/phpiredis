Installing
----------
You need to have `libhiredis` pre-installed. You can fetch the latest version
from https://github.com/redis/hiredis.

    git clone https://github.com/seppo0010/phpiredis.git
    cd phpiredis

On some installs, you might need to install `php-dev`:

    sudo apt-get install php5-dev

Configure and build the extension:

    phpize
    ./configure --enable-phpiredis --with-hiredis-dir=/usr/local
    make

Copy the extension (modules/phpiredis.so) to your php extensions directory and
edit your php.ini to load by default.

Run the tests and [create an issue][] on github if some of the tests fail.

    make test

  [create an issue]: http://github.com/seppo0010/phpiredis/issues

Usage
-----
To connect to the redis server just call `phpiredis_connect` with the server ip
as the first parameter:

    $link = phpiredis_connect('127.0.0.1');

You can also specify a port as a second parameter, but by default it will use
redis default (6379):

    $link = phpiredis_connect('127.0.0.1', 6380);

You can execute commands with:

    phpiredis_command($link, 'DEL test');
    phpiredis_command($link, 'SET test 1');

Or even multiple commands with:

    phpiredis_multi_command($link, array('DEL test', 'SET test 1', 'GET test'));

If you need a binary safe commands you must use the `_bs` variants of `phpiredis_command`
and `phpiredis_multi_command` by providing each command expressed as an array composed
by its arguments:

    phpiredis_command_bs($link, array('DEL','test'));
    phpiredis_multi_command_bs($link, array(array('SET', 'test', '1'), array('GET', 'test')));

Check the TODO to view the status.

Contributing
------------
Downloading and testing is really welcome! Also fork the project on github and
help with the todo list. For new features please send me a message on github or
in the IRC [#redis][] channel in [freenode][].

  [#redis]: http://webchat.freenode.net/?channels=redis
  [freenode]: http://freenode.net/

Author
------
Phpiredis was written by Sebastian Waisbrot (seppo0010 at gmail) and is
released under the BSD license. For details about Hiredis license, please
check lib/hiredis.
