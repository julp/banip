# vmod_msgsend

Send POSIX or System V messages from Varnish 4 to another process

## Synopsis

`import msgsend [from "path"];`

## Prerequisites

### FreeBSD

POSIX message queue file system support enabled in kernel:
* kernel compiled with: `options P1003_1B_MQUEUE`
* (or) loading kernel module:
    + manually: `kldload mqueuefs`
    + automatically: `echo 'mqueuefs_load="YES"' >> /boot/loader.conf`

Optional (convenient), mount mqueuefs to see current POSIX queues in `/mnt/mqueue/`:
* manually: `mount -t mqueuefs null /mnt/mqueue`
* (or) automatically: `echo 'null    /mnt/mqueue     mqueuefs         rw      0       0' >> /etc/fstab`

## Installation

First, you'll need:
* varnish sources (same exact version)
* cmake

Then, to build this module:
```
cd .../banip/clients/varnish4
cmake . -DVARNISHSRC:PATH=/path/to/varnish/sources # or build it into an other directory
make
(sudo) make install
```

## Usage

```
vcl 4.0;

import msgsend; # from "/a/non/standard/place/"

sub vcl_init {
    new banip = msgsend.mqueue("/banip");
}

sub vcl_recv {
    if (...) {
        banip.sendmsg("" + client.ip); # sadly "" + is needed for explicit cast from ip to string
    }
}
```
