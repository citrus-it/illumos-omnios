How to build unleashed
======================

Install unleashed
-----------------

Building unleashed requires unleashed, so first you must install it (doing
which is not documented at the time of writing).

Get the source
--------------

Clone this repository from either:

* git://repo.or.cz/unleashed.git
* http://repo.or.cz/unleashed.git

For example:

```
$ git clone git://repo.or.cz/unleashed.git
$ cd unleashed
```

Build
-----

For a complete build, use 'tools/nightly.sh'. The user executing the build
needs to be able to write to `/usr/obj`, so make sure you can do that first.

```
# install -m 1777 -d /usr/obj
$ ./tools/nightly.sh
```

On success, this results in installable packages in the `packages` directory.

Incremental build
-----------------

To build a component that is using the new build system (e.g., cat(1)), change
into the source directory and run make. For example:

```
$ cd bin/cat
$ make
# make install
```

The component will be built against the running system and installed to /. To
install to the proto area, pass DESTDIR=path to 'make install'. Building
against libraries and headers in the "proto area" or in object directories is
not currently supported.

To build a component under the legacy (dmake) build system, ie. things under
'usr/src', first complete a full nightly build so that prerequisite objects for
the component are made, and then:

```
$ ./tools/bldenv.sh <env file>
$ cd usr/src/cmd/w
$ dmake install
```

The component will be built against the "proto area" and installed there.
