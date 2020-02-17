GOFISH AND CGI
--------------

WARNING: The CGI is currently very experimental. Since this runs
         programs external to GoFish, it is not as secure. I am only
         adding it for viewcvs on my site.

First, CGI is only available to the http server. If you are using
GoFish as a gopher only server, you might as well leave it off. CGI is
disabled by default. To enable it, type `configure --enable-cgi' and
recompile.

GoFish produces the minimal CGI needed to get viewcvs running. The
scripts must be in `<root>/cgi-bin'. This is currently hardcoded. If
you use virtual hosts, they all use the same top-level cgi-bin.

CHROOT
------

GoFish runs in a chroot environment. This means that to run programs, perl, or
python scripts, all the libraries and programs needed to run the
script must exist inside the jail. They cannot be links.
