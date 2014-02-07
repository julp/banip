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

COPYRIGHT
=========

::

  Module msgsend
  Init init_function
  Function VOID init_msgsend(PRIV_VCL, STRING)
  Function VOID msgsend(PRIV_VCL, STRING)

