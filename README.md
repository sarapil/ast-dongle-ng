# ast-dongle-ng

This repository provides an experimental skeleton of the `chan_dongle_ng` Asterisk channel driver.  It is a starting point for a modern driver that replaces the legacy `chan_dongle` module.  The project is intentionally kept simple and uses a plain `Makefile` for building the shared object.

Currently the code only implements a minimal channel driver that registers with Asterisk and exposes one CLI command:

```
asterisk -rx "dongle_ng reset <device>"
```

The command only logs a message; real reset logic needs to be implemented.

To build the module run `make`.  The resulting `chan_dongle_ng.so` can be placed in Asterisk's modules directory.
A working Asterisk development environment with headers is required to compile.
