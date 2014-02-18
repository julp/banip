============
vmod_msgsend
============

SYNOPSIS
========

import msgsend [from "path"] ;

DESCRIPTION
===========

Send POSIX messages from Varnish 4 to another process

PREREQUISITES
=============

FreeBSD
-------

1. POSIX message queue file system support enabled in kernel:
    - kernel compiled with: `options P1003_1B_MQUEUE`
    - loading kernel module:
        + manually: `kldload mqueuefs`
        + automatically: `echo 'mqueuefs_load="YES"' >> /boot/loader.conf`
2. Mount mqueuefs at the /mnt/mqueue
    - manually: `mount -t mqueuefs null /mnt/mqueue`
    - automatically: `echo 'null    /mnt/mqueue     mqueuefs         rw      0       0' >> /etc/fstab`

INSTALLATION
==================

Installation requires Varnish source tree.

Usage::

 ./autogen.sh
 ./configure VARNISHSRC=DIR [VMODDIR=DIR]

`VARNISHSRC` is the directory of the Varnish source tree for which to
compile your vmod. Both the `VARNISHSRC` and `VARNISHSRC/include`
will be added to the include search paths for your module.

Optionally you can also set the vmod install directory by adding
`VMODDIR=DIR` (defaults to the pkg-config discovered directory from your
Varnish installation).

Make targets:

* make - builds the vmod
* make install - installs your vmod in `VMODDIR`
* make check - runs the unit tests in ``src/tests/*.vtc``

CONTENTS
========

* :ref:`obj_mqueue`
* :ref:`func_mqueue.sendmsg`

.. _obj_mqueue:

Object mqueue
=============

.. _func_mqueue.sendmsg:

VOID mqueue.sendmsg(STRING)
---------------------------

Prototype
    VOID mqueue.sendmsg(STRING)
