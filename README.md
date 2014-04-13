# vmod_msgsend

Send POSIX messages from Varnish 4 to another process

## Synopsis

`import msgsend [from "path"];`

## Prerequisites

### FreeBSD

1. POSIX message queue file system support enabled in kernel:
    * kernel compiled with: `options P1003_1B_MQUEUE`
    * (or) loading kernel module:
        + manually: `kldload mqueuefs`
        + automatically: `echo 'mqueuefs_load="YES"' >> /boot/loader.conf`
2. Mount mqueuefs at the /mnt/mqueue
    * manually: `mount -t mqueuefs null /mnt/mqueue`
    * (or) automatically: `echo 'null    /mnt/mqueue     mqueuefs         rw      0       0' >> /etc/fstab`

## Installation

First, you'll need:
* varnish sources (same exact version)
* cmake

Then, to build this module:
```
cd .../libvmod-msgsend/
cmake . -DVARNISHSRC:PATH=/path/to/varnish/sources # or build it into an other directory
make
(sudo) make install
```

## Usage

```
import msgsend;

sub vcl_init {
    new queue = msgsend.mqueue("/foo");
}

sub vcl_recv {
    queue.sendmsg("" + client.ip + " asks " + req.url);
}
```

For real applications, see content of `consumers/` subdirectory.

For example, by linking it to [a redis or memcached backend](https://github.com/julp/libvmod-keystore), you can dynamically (ip) ban spammers and/or prevent brute-force.
