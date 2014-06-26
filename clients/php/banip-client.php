<?php
/*
Required extension: sysvmsg
Note: banipd needs to be built with System V queue (not POSIX)

Example:
$banip = new BanIPClient('/tmp/banip');
$banip->addAddress($_SERVER['REMOTE_ADDR']);
*/

class BanIPClient
{
    private $_queue;

    public function __construct($path) {
        $id = unpack('c', file_get_contents($path));
        $key = ftok($path, chr($id[1]));
        $this->_queue = msg_get_queue($key);
    }

    public function addAddress($addr) {
        return msg_send($this->_queue, 1, $addr, FALSE, FALSE, $errno);
    }
}
