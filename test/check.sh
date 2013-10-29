#!/bin/bash

# This script is really just a wrapper for Travis CI to call in order
# to run the tests in wine when cross-compiling with mingw.  Using it
# anywhwere else is unsupported.

case "${CC}" in
		"i686-w64-mingw32-gcc")
				ENV=wine
				;&
		"x86_64-w64-mingw32-gcc")
				ENV=wine64
				MAKE_ARGS="WINDOWS=1"
				EXT=".exe"
				;;
esac

make CC="${CC}" ${MAKE_ARGS}
${ENV} ./test${EXT}
