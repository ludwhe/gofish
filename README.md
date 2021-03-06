# GoFish

[![C/C++ CI](https://github.com/ludwhe/gofish/workflows/C/C++%20CI/badge.svg)](https://github.com/ludwhe/gofish/actions?query=workflow%3A%22C%2FC%2B%2B+CI%22)

## What is this?

GoFish is my attempt to boost the performance of the current
gopherds. However, I do have some design goals. I am running GoFish on
my 'Winder <http://seanm.ca/>. While it is a great machine, it is not
the most powerful machine in the world.

### Design Goals

1) Secure. I do not plan on letting the server run executables.

2) Fast. At least as fast as a web server. I am using thttpd as my
  benchmark.

3) Low resource usage. Single process.

4) Simple. The daemon will only serve files. Directory listings will
  be read from the .cache files verbatim. I think this is an
  important decision. It gives maximum flexibility to the
  users. They can go from the exteme of writing all the cache files
  by hand, to having some form of custom, fully automated file
  insertion program. I plan on providing one such program called
  `mkcache', but it will not be required.

## Where can I get it?

GoFish is hosted on SourceForge. Go to
[the project page](http://gofish.sourceforge.net)
and follow the instructions.

GoFish has also been forked on GitHub. Go to
[the project page](http://github.com/ludwhe/gofish)
and follow the instructions.

## Current assumptions

I am currently using the Linux mmap(2) call to write out the
files. Because of this, I do not process the files. The following
assumption must hold:

**Files must not have any lines containing only a ".".**

There is a script called check-files that will verify that the above
is true. If you have changed the GOPHER_ROOT, you must change it in
the check-files script also.

## How to build it

### From RPMs

If you are using the RPMS, you don't have to build it! Just install
the gofish rpm. If this is a first time install, backup anything in
your gopher root directory and install the gofish-setup rpm.

### From tarball

1) Untar the tarball. You should get a gofish-`version` directory.
2) cd gofish-`version`
3) `./configure`
    or
  `./configure --disable-http`
    or for a redhat style install
  `./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var`
4) `make`
5) `sudo make install`

## Optional features

GoFish contains a gopher to http gateway. This feature is enabled by
default. To disable the feature:

```sh
./configure --disable-http
```

The http gateway will honour favicon.ico requests. Drop a favicon.ico
file in the gopher root. mkcache ignores this file.

## Log rotation

GoFish supports log rotation. Move the log to a new name. Then send a
USR1 signal to GoFish. It will close the old log and create a new
file. No log entries will be lost. After the signal, the old log
should be up to date.

Example: (note that kill line uses backwards ticks, not single quotes)

```sh
mv /var/log/gopherd.log /var/log/gopherd.log.1
kill -USR1 `cat /var/run/gopherd.pid`
```

## Contact

You can contact me about problems/suggestions/donations.

  Sean MacLennan
  headgopher@seanm.ca
