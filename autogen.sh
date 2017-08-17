#!/bin/sh

# Keep this in version control in case for whatever reason we lose
# depcomp, et al
autoreconf --install
automake --add-missing --copy >/dev/null 2>&1
