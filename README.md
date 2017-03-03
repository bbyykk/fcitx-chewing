fcitx-taigi

## Prerequisite

Please installl the libtaigi first!
Install the following package:

	# sudo apt-get install fcitx-libs-dev

## Build

	# cd fcitx-taigi && cd fcitx-taigi
	# mkdir build && cd build
	# cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_VERBOSE_MAKEFILE=ON -DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_SYSCONFDIR=/etc -DCMAKE_INSTALL_LOCALSTATEDIR=/var
	# make

## Install
Under the fcitx-taigi/build

	# sudo make install
