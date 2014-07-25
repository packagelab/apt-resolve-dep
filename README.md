# apt-resolve-dep

Given a [Debian control file][debian-control-file], resolve all build
dependencies using the same algorithm as `apt-get build-dep`. Print
the required packages to `stdout` one-per-line in a format that can be
passed directly to `apt-get install`:

    package:arch=version

## Build instructions

    $ make

## Example

    $ apt-get -qqd source strace
    $ apt-resolve-dep strace_*.dsc
    autoconf:all=2.69-6
    automake:all=1.14.1-2ubuntu1
    automake1.11:all=1.11.6-2
    autopoint:all=0.18.3.1-1ubuntu3
    autotools-dev:all=20130810.1
    bsdmainutils:amd64=9.0.5ubuntu1
    debhelper:all=9.20131227ubuntu1
    dh-apparmor:all=2.8.95~2430-0ubuntu5
    dh-autoreconf:all=9
    gettext:amd64=0.18.3.1-1ubuntu3
    gettext-base:amd64=0.18.3.1-1ubuntu3
    groff-base:amd64=1.22.2-5
    intltool-debian:all=0.35.0+20060710.1
    libasprintf0c2:amd64=0.18.3.1-1ubuntu3
    libcroco3:amd64=0.6.8-2ubuntu1
    libglib2.0-0:amd64=2.40.0-2
    libpipeline1:amd64=1.3.0-1
    libsigsegv2:amd64=2.10-2
    libtool:amd64=2.4.2-1.7ubuntu1
    libunistring0:amd64=0.9.3-5ubuntu3
    libxml2:amd64=2.9.1+dfsg1-3ubuntu4.3
    m4:amd64=1.4.17-2ubuntu1
    man-db:amd64=2.6.7.1-1
    po-debconf:all=1.0.16+nmu2ubuntu1

## Details

Suppose you have a [control file][debian-control-file] for a Debian
source package. There's no simple way to print the exact list of
dependency packages required to build it on your system. The `apt-get
build-dep` comes close, but it (1) requires the name of a source
package as input, rather than a path to a control file, and (2)
installs the dependency packages, rather than printing them.

People have invented various hacks to work around this. A common
approach is to create a dummy source package from the control file,
install it, and then run `apt-get build-dep`. This is what the
official Debian [buildd][buildd] system does. See also the
[mk-build-deps][mk-build-deps] program.

And so `apt-resolve-dep` was born. It's based on code from the
[apt][apt] 1.0.5 codebase. It vendors `libapt-pkg` rather than linking
against it so that the only runtime dependencies are `libc`, `libgcc`,
`libstdc++`, and `zlib`. It uses the same dependency-finding algorithm
that `apt-get build-dep` uses.

## TODO

* Handle comments in control files
* Show error when opening a directory
* Test suite

[apt]: http://anonscm.debian.org/cgit/apt/apt.git
[buildd]: https://wiki.debian.org/buildd
[debian-control-file]: https://www.debian.org/doc/debian-policy/ch-controlfields.html#s-sourcecontrolfiles
[mk-build-deps]: http://manpages.ubuntu.com/manpages/trusty/man1/mk-build-deps.1.html
