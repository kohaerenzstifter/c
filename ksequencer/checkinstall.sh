#!/bin/sh

checkinstall --pkgname=ksequencer --pkgsource=ksequencer --install=no \
	--maintainer=sancho@posteo.de --pkggroup=multimedia --pkgversion=$1 \
	--pkgrelease=$2 --requires=libglib2.0-0,libgtk-3-0,libgdk-pixbuf2.0-0,libasound2 \
	--backup=no --fstrans=yes
