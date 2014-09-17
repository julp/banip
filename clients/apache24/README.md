(build queue library first)

```
cd .../clients/apache24
apxs -i -a -I../../queues -L../../queues -lqueue -c mod_banip.c
```

`service apache2 restart`/`systemctl restart apache2`

* `BanIPEnable` on/off (default: off)
* `BanIPQueue` name of the queue
* `BanIPRule` [variable] pattern

Request ban from PHP (through output filter), just add a X-BanIP header (`header('X-BanIP: true');`).

Examples of ban request:
* `BanIPRule /(?:php-)?cgi/` if path contains /php-cgi/ or /cgi/
* `BanIPRule GET:option .` if query string contains any *option* parameter in query string
* `BanIPRule HTTP:Host =localhost` if requested Host is localhost
