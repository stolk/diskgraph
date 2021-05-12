# diskgraph
Monitor for disk IO

![screenshot](images/screenshot0.png "screenshot")


## Introduction

The diskgraph tool will graph disk IO under linux, in a terminal.

Examples:

**$ ./diskgraph /dev/nvme0n1**


**$ ./diskgraph /dev/sda**

You can also leave out the `dev` prefix and do:

**$ ./diskgraph sda**


Periodically (500ms intervals) it will read the statistics from /sys/block/DEVICE/stat and see how many sectors are read and written since last sample.

These are converted to bandwidth, by dividing by time.

It also shows (in orange) the number of operations that are in-flight at the moment the sample was taken.

## Keys

Press ESCAPE or Q to exit diskgraph.

## Known issues

* Shows garbage on terminals that do not support 24 bit colour.
* Does not work on older linux kernels.
* Missing manual page.
* Assumes 512 byte sectors.

## Copyright

diskgraph is (c)2021 by Bram Stolk and licensed under the MIT license.
Thank you to PQCraft for improving resize functionality and other changes.
