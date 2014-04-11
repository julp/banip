=====
pfban
=====

USAGE
=====

::

    pfban [options] <queue name> <PF table name>

-v             verbose
-d             daemonize
-l <filename>  logfile (default: stderr)
-p <filename>  pidfile (default: none)
-g <group>     group to run as
-b <size>      maximum messages size (in bytes)
-s <size>      maximum messages in queue

INSTALLATION
============

::

    cc -lrt pfban.c -o pfban

- Create a table in your pf.conf (eg: `table <blacklist> persist file "/etc/pf.table.blacklist"`)
- Block trafic from those adresses (`block quick from <blacklist>`)

BEST PRACTICES
==============

- create a specific group and run varnish and this program under it
- add a `pass quick` rule prior to whitelist one or more IP you may use to ensure an access to your host

::

    table <whitelist> persist { ip1 ip2 }
    table <blacklist> persist file "/etc/pf.table.blacklist"
    #
    # Default behavior
    block log all
    # Allow at least SSH access
    pass in quick log on ... inet proto tcp from <whitelist> ... port ssh
    # Deny all suspicious hosts
    block quick from <blacklist>
    # ...

- send USR1 signal when rotating log

/etc/newsyslog.conf:

::

    /.../pfban.log root:wheel 600 <other attributes> /.../pfban.pid 30


VCL EXAMPLE
===========

::

    vcl 4.0;
    #
    import msgsend; # from "/a/non/standard/place/"
    #
    sub vcl_init {
        new pfban = msgsend.mqueue("/pfban");
    }
    #
    sub vcl_recv {
        if (...) {
            pfban.sendmsg("" + client.ip); # "" + is needed for explicit cast from ip to string
        }
    }
