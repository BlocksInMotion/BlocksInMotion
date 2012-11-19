#!/bin/sh

BLOCKSINMOTION_OS="unknown"
BLOCKSINMOTION_PLATFORM="x32"
BLOCKSINMOTION_MAKE="make"
BLOCKSINMOTION_MAKE_PLATFORM="32"
BLOCKSINMOTION_ARGS=""
BLOCKSINMOTION_CPU_COUNT=1
BLOCKSINMOTION_USE_CLANG=1

for arg in "$@"; do
	case $arg in
		"gcc")
			BLOCKSINMOTION_ARGS+=" --gcc"
			BLOCKSINMOTION_USE_CLANG=0
			;;
		"cuda")
			BLOCKSINMOTION_ARGS+=" --cuda"
			;;
		*)
			;;
	esac
done

if [[ $BLOCKSINMOTION_USE_CLANG == 1 ]]; then
	BLOCKSINMOTION_ARGS+=" --clang"
fi

case $( uname | tr [:upper:] [:lower:] ) in
	"darwin")
		BLOCKSINMOTION_OS="macosx"
		BLOCKSINMOTION_CPU_COUNT=$(sysctl -a | grep 'machdep.cpu.thread_count' | sed -E 's/.*(: )([:digit:]*)/\2/g')
		;;
	"linux")
		BLOCKSINMOTION_OS="linux"
		BLOCKSINMOTION_CPU_COUNT=$(cat /proc/cpuinfo | grep -m 1 'cpu cores' | sed -E 's/.*(: )([:digit:]*)/\2/g')
		if [[ $(cat /proc/cpuinfo | grep -m 1 "flags.* ht " | wc -l) == 1 ]]; then
			BLOCKSINMOTION_CPU_COUNT=$(expr ${BLOCKSINMOTION_CPU_COUNT} + ${BLOCKSINMOTION_CPU_COUNT})
		fi
		;;
	[a-z0-9]*"BSD")
		BLOCKSINMOTION_OS="bsd"
		BLOCKSINMOTION_MAKE="gmake"
		;;
	"cygwin"*)
		BLOCKSINMOTION_OS="windows"
		BLOCKSINMOTION_ARGS+=" --env cygwin"
		BLOCKSINMOTION_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		;;
	"mingw"*)
		BLOCKSINMOTION_OS="windows"
		BLOCKSINMOTION_ARGS+=" --env mingw"
		BLOCKSINMOTION_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		;;
	*)
		echo "unknown operating system - exiting"
		exit
		;;
esac


BLOCKSINMOTION_PLATFORM_TEST_STRING=""
if [[ $BLOCKSINMOTION_OS != "windows" ]]; then
	BLOCKSINMOTION_PLATFORM_TEST_STRING=$( uname -m )
else
	BLOCKSINMOTION_PLATFORM_TEST_STRING=$( gcc -dumpmachine | sed "s/-.*//" )
fi

case $BLOCKSINMOTION_PLATFORM_TEST_STRING in
	"i386"|"i486"|"i586"|"i686")
		BLOCKSINMOTION_PLATFORM="x32"
		BLOCKSINMOTION_MAKE_PLATFORM="32"
		BLOCKSINMOTION_ARGS+=" --platform x32"
		;;
	"x86_64"|"amd64")
		BLOCKSINMOTION_PLATFORM="x64"
		BLOCKSINMOTION_MAKE_PLATFORM="64"
		BLOCKSINMOTION_ARGS+=" --platform x64"
		;;
	*)
		echo "unknown architecture - using "${BLOCKSINMOTION_PLATFORM}
		exit;;
esac


echo "using: premake4 --cc=gcc --os="${BLOCKSINMOTION_OS}" gmake "${BLOCKSINMOTION_ARGS}

premake4 --cc=gcc --os=${BLOCKSINMOTION_OS} gmake ${BLOCKSINMOTION_ARGS}
sed -i -e 's/\${MAKE}/\${MAKE} -j '${BLOCKSINMOTION_CPU_COUNT}'/' Makefile

if [[ $BLOCKSINMOTION_USE_CLANG == 1 ]]; then
	sed -i '1i export CC=clang' Makefile
	sed -i '1i export CXX=clang++' Makefile
fi

echo ""
echo "###################################################"
echo "# NOTE: use '"${BLOCKSINMOTION_MAKE}"' to build BlocksInMotion"
echo "###################################################"
echo ""
