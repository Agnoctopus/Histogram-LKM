Histogram LKM
============

Histogram is a Linux Kernel Module which provides a histogram of all
words written since the loading of the module.

Histogram works by a registering a keyboard notifier to keep track of
keys pressed. Only ascii characters work during this period.

The histogram is available through a file in debugfs, it contains all
words typed since the loading of the Histogram LKM:
`/sys/kernel/debugfs/histogram/histogram` (Could be accessed in
privileged mode).

# Usage

To load the module:
```shell
$ sudo make load
```

To unload the module:
```shell
$ sudo make unload
```

To install the module (run as root):
```shell
# make install
```
> If you want to sign the module: https://wiki.gentoo.org/wiki/Signed_kernel_module_support

To view the histogram:
```shell
$ cat /sys/kernel/debugfs/histogram/histogram
not: 1
to: 2
or: 1
be: 2
```

To get the module info:
```shell
$ modinfo histogram
filename:       /lib/modules/5.6.2-arch1-2/extra/histogram.ko.xz
version:        0.1
description:    Histogram of written words.
author:         Cesar Belley <cesar.belley@lse.epita.fr>
license:        GPL v2
srcversion:     2412D2C2AA86A5ADFCCA15E
depends:
retpoline:      Y
name:           histogram
vermagic:       5.6.2-arch1-2 SMP preempt mod_unload
```

# Building

Build the binary:
```shell
$ make
```

Generate doc:
```
$ make doc
```

# License

This project is licensed under GPL-2.0.

# Author

César `MattGorko` Belley <cesar.belley@lse.epita.fr>
