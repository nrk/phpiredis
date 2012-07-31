Installing
----------
You need to have libhiredis pre-installed. You can fetch the latest version from https://github.com/antirez/hiredis.git
Clone the git repository ("git clone git://github.com/seppo0010/phpiredis.git") and go to the new folder ("cd phpiredis").
On some installs, you might need to install php-dev (In Ubuntu "sudo apt-get install php5-dev")
Configure and build the extension ("phpize && ./configure  --enable-phpiredis --with-hiredis-dir=/usr/local && make").
Copy the extension (modules/phpiredis.so) to your php extensions directory and edit your php.ini to load by default.
Run the tests ("make test"). If some of the test fails, please create an issue on github (http://github.com/seppo0010/phpiredis/issues).


Usage
-----
To connect to the redis server just call phpiredis_connect with the server ip as the parameter. You can also specify a port as a second parameter, but by default it will use redis default (6379).
You can simple commands with
phpiredis_command($link, 'DEL test');
phpiredis_command($link, 'SET test 1');

Or even multiple commands with
phpiredis_multi_command($link, array('DEL test', 'SET test 1', 'GET test'));

If you need a binary safe command you must use the following
phpiredis_command_bs($link, array('DEL','test'));
Important: note that each argument is an element in an array.

Check the TODO to view the status.

Contributing
------------
Downloading and testing is really welcome!
Also fork the project on github and help with the todo list.
For new features please send me a message on github or in the IRC #redis channel in freenode.

Author
------
Phpiredis was written by Sebastian Waisbrot (seppo0010 at gmail) and is released under the BSD license.
For details about Hiredis license, please check lib/hiredis.
