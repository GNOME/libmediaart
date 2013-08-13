#!/bin/sh
# Run this to generate all the initial makefiles, etc.
#
# NOTE: compare_versions() is stolen from gnome-autogen.sh

# Generate required files
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
(
  cd "$srcdir" &&
  touch ChangeLog && # Automake requires that ChangeLog exist
  autopoint --force &&
  AUTOPOINT='intltoolize --automake --copy' autoreconf --verbose --force --install
) || exit
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
