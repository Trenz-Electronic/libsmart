#! /bin/sh

# Source: https://stackoverflow.com/questions/33278928/how-to-overcome-aclocal-1-15-is-missing-on-your-system-warning
#

# Often, you don't need any auto* tools and the simplest solution is to simply run touch aclocal.m4 configure in the relevant folder (and also run touch on Makefile.am and Makefile.in if they exist). This will update the timestamp of aclocal.m4 and remind the system that aclocal.m4 is up-to-date and doesn't need to be rebuilt. After this, it's probably best to empty your build directory and rerun configure from scratch after doing this. I run into this problem regularly. For me, the root cause is that I copy a library (e.g. mpfr code for gcc) from another folder and the timestamps change.

# Of course, this trick isn't valid if you really do need to regenerate those files, perhaps because you have manually changed them. But hopefully the developers of the package distribute up-to-date files.


FILES="aclocal.m4 configure Makefile.am Makefile.in"
echo touch "$FILES"
touch $FILES

