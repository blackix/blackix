# Fix Mono and engine dependencies if needed
START_DIR=`pwd`
cd "$1"
sh FixMonoFiles.sh
sh FixDependencyFiles.sh

IS_MONO_INSTALLED=0
if [ -f "/usr/bin/mono" ]; then
	# If Mono is installed, check if it's 3.2.6 or higher
	MONO_VERSION_PREFIX="Mono JIT compiler version "
	MONO_VERSION_PREFIX_LEN=${#MONO_VERSION_PREFIX}
	MONO_VERSION=`/usr/bin/mono --version |grep "$MONO_VERSION_PREFIX"`
	MONO_VERSION=(`echo ${MONO_VERSION:MONO_VERSION_PREFIX_LEN} |tr '.' ' '`)
	if [ ${MONO_VERSION[0]} -ge 3 ]; then
		if [ ${MONO_VERSION[1]} -eq 2 ] && [ ${MONO_VERSION[2]} -ge 6 ]; then
			IS_MONO_INSTALLED=1
#		elif [ ${MONO_VERSION[1]} -gt 2 ]; then # @todo: enable this when problems with running on Mono 3.4 are solved
#			IS_MONO_INSTALLED=1
		fi
	fi
fi

# Setup bundled Mono if cannot use installed one
if [ $IS_MONO_INSTALLED -eq 0 ]; then
	echo Setting up Mono
	CUR_DIR=`pwd`
	export UE_MONO_DIR=$CUR_DIR/../../../Binaries/ThirdParty/Mono/Mac
	export PATH=$UE_MONO_DIR/bin:$PATH
	export MONO_PATH=$UE_MONO_DIR/lib:$MONO_PATH
	export LD_LIBRARY_PATH=$UE_MONO_DIR/lib:$LD_LIBRARY_PATH

	if [ ! -f $UE_MONO_DIR/lib/libmsvcrt.dylib ]; then
		ln -s /usr/lib/libc.dylib $UE_MONO_DIR/lib/libmsvcrt.dylib
	fi
fi

cd "$START_DIR"
