#!/bin/sh
# Run this to generate all the initial makefiles, etc.

# Generate required files
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
(
  touch ChangeLog # Required by automake.
  autoreconf --verbose --force --install
) || exit

# Run configure
"$srcdir/configure" "$@"
