#!/bin/bash

# ----------------------------------------------------------------------------------------
# Copyright (c) 2012 Marcus Geelnard
#
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
#     1. The origin of this software must not be misrepresented; you must not
#     claim that you wrote the original software. If you use this software
#     in a product, an acknowledgment in the product documentation would be
#     appreciated but is not required.
#
#     2. Altered source versions must be plainly marked as such, and must not be
#     misrepresented as being the original software.
#
#     3. This notice may not be removed or altered from any source
#     distribution.
# ----------------------------------------------------------------------------------------

# Name of the distribution
distname=TinyCThread-1.1

# Build all the necessary files
echo Building documentation...
cd doc
rm -f html/*
doxygen
cd ..

# Set up a temporary directory
tmproot=/tmp/tinycthread-$USER-$$
mkdir $tmproot
tmpdir=$tmproot/$distname
mkdir $tmpdir

# Copy files
echo Copying files to $tmpdir...
mkdir $tmpdir/source
mkdir $tmpdir/test
mkdir $tmpdir/doc
mkdir $tmpdir/doc/html
cp *.txt $tmpdir/
cp source/*.h source/*.c $tmpdir/source/
cp test/Makefile* test/*.h test/*.c $tmpdir/test/
cp doc/Doxyfile $tmpdir/doc
cp doc/html/* $tmpdir/doc/html

# Create archives
olddir=`pwd`
cd $tmproot
tar -cvf $distname-src.tar $distname
bzip2 -9 $distname-src.tar
zip -9r $distname-src.zip $distname
cd $olddir
cp $tmproot/*.bz2 $tmproot/*.zip ./

# Remove temporary directory
rm -rf $tmproot

