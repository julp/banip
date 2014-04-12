# pfban

## Usage

`pfban [options] -q <queue name> -t <table name>`

* `-v/--verbose`: be verbose
* `-d/--daemonize`: daemonize (default: off)
* `-e/--engine <engine>`: name of the firewall to use (optional except for NetBSD if PF and NPF are both enabled)
* `-l/--log <filename>`: logfile (default: stderr)
* `-p/--pid <filename>`: pidfile (default: none)
* `-q/--queue <queue name>`: name of the queue
* `-g/--group <group>`: name of the group to run as
* `-b/--msgsize <size>`: maximum messages size (in bytes) (default: 1024)
* `-s/--qsize <size>`: maximum messages in queue (default: 10)
* `-t/--table <table name>`: name of the table/set

## Supported firewalls

### PF: (OpenBSD) Packet filter

* Create a table in your pf.conf (eg: `table <blacklist> persist file "/etc/pf.table.blacklist"`)
* Block trafic from those adresses (`block quick from <blacklist>`)

### NPF (NetBSD >= 6.0)

* Create a table in your npf.conf (eg: `table <blacklist> type hash dynamic`)
* Block trafic from those adresses (`block in final from <blacklist>`)

WARNING: for now, NPF gives a numeric identifier to tables but they could change when reloading your rules.

Table names are only supported in NetBSD-current.

### nftables (Linux >= 3.13)

In the future? (status: incomplete)

## Prerequisites

* cmake
* bash for tests

## Installation

```
cd .../libvmod-msgsend/consumers/pfban
cmake . # or build it into an other directory
make
(sudo) make install
```

## Best practices

### Who can send (write) message

Create a specific group and run varnish and this program under it (in fact, this is mandatory, else you'll get "permission denied" error, because queue is not writable by "others")

### Always keep an access to your host

Add a `pass quick` (or PF equivalent) rule prior to whitelist one or more IP you may use to ensure an access to your host:

```
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
```

### Rotating log

Send a USR1 signal when rotating log

/etc/newsyslog.conf:
```
/.../pfban.log root:wheel 600 <other attributes> /.../pfban.pid 30
```

## Interface with Varnish

```
vcl 4.0;

import msgsend; # from "/a/non/standard/place/"

sub vcl_init {
    new pfban = msgsend.mqueue("/pfban");
}

sub vcl_recv {
    if (...) {
        pfban.sendmsg("" + client.ip); # "" + is needed for explicit cast from ip to string
    }
}
```
