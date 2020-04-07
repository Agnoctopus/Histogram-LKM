Histogram LKM
============

Histogram is a Linux Kernel Module that provide an histogram of all
written words since the module loading.

Histogram works by a registering a keyboard notifier to keep tracks of
key pressed. Only ascii characters works during this time.

The histogram is available through a file in debugfs, it contain all
words types since the Histogram LKM loading:
/sys/kernel/debugfs/histogram/histogram. (Could be accessed in
privileged mode).

# Usage

To load the module:
```shell
sudo make load
```

To unload the module:
```shell
sudo make unload
```

To install the module (run as root):
```shell
make install
```
> If you want to sign the module: https://wiki.gentoo.org/wiki/Signed_kernel_module_support

To view the histogram:
```shell
cat /sys/kernel/debugfs/histogram/histogram
```

To get the module info:
```shell
modinfo histogram
```

# Building

Build the binary:
```shell
make
```

Generate doc:
```
make doc
```

# License

This project is licensed under GPL-2.0.

# Author

César `MattGorko` Belley <cesar.belley@lse.epita.fr>
