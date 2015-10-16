#!/bin/bash

case "${1}" in
    "deps")
	case "${TRAVIS_OS_NAME}" in
	    "linux")
		sudo apt-get update -qq
		sudo apt-get install -qq python-software-properties
                sudo apt-add-repository -y ppa:ubuntu-toolchain-r/test
                sudo apt-get update -qq

		case "${COMPILER}" in
		    "gcc-5")
                        sudo apt-get install -qq gcc-5
                        ;;
                    "gcc-4.6")
                        sudo apt-get install -qq gcc-4.6
                        ;;
                    "gcc-4.8")
                        sudo apt-get install -qq gcc-4.8
                        ;;
                    "tcc")
                        sudo apt-get install -qq tcc
                        ;;
		    "i686-w64-mingw32-gcc")
			sudo apt-get install -qq gcc-mingw-w64-i686 wine1.4
			;;
		    "x86_64-w64-mingw32-gcc")
			sudo apt-get install -qq gcc-mingw-w64-x86-64 wine1.4
			;;
		esac
		;;
	    "osx")
		brew update

		case "${COMPILER}" in
		    "gcc-4.6")
                        which gcc-4.6 || brew install homebrew/versions/gcc46
                        ;;
		    "gcc-4.8")
                        which gcc-4.8 || brew install homebrew/versions/gcc48
                        ;;
		    "gcc-5")
                        which gcc-5 || brew install homebrew/versions/gcc5
                        ;;
		    "tcc")
			which tcc || brew install tcc
			;;
		esac
		;;
	esac
	;;

    "build")
	case "${COMPILER}" in
	    "i686-w64-mingw32-gcc")
		MAKE_ARGS="WINDOWS=1"
		;;
	    "x86_64-w64-mingw32-gcc")
		MAKE_ARGS="WINDOWS=1"
		;;
	esac

	(cd test && make CC="${COMPILER}" ${MAKE_ARGS})
	;;

    "test")
	case "${COMPILER}" in
	    "i686-w64-mingw32-gcc")
		ENV=wine
		EXT=".exe"
		;;
	    "x86_64-w64-mingw32-gcc")
		ENV=wine64
		EXT=".exe"
		;;
	esac

	${ENV} ./test/test${EXT}
	;;
esac
