#!/bin/bash

###############################################################################
# CONFIGURATION:
###############################################################################
COMPILER="$PWD"/tools/arduino-cli

SERIAL_PORT=COM4
MONITOR_BAUD=115200
OTA_IP="10.10.10.10"

TARGET=leaf_can_filter_esp32c6_hw1

#EXTRA_FLAGS="-v"

###############################################################################
# TARGETS AVAILABLE
###############################################################################

if [ "$TARGET" == "leaf_can_filter_esp32c6_hw1" ]; then
	BOARD=esp32:esp32:esp32c6
	FQBN=:CDCOnBoot=cdc
else
	echo "Bad target!"
	exit 1
fi

###############################################################################
# MAIN
###############################################################################
# Set environment variables
if [ -z "${GIT_REPO_VERSION+x}" ]; then
	export GIT_REPO_VERSION=$(git describe --tags)
fi

compile() {
	# enumerate (and empty) all generated files
	mkdir -p build

	rm build/* 2> /dev/null # Clean build
	
	# Setup tools and libraries
	./setup.sh
	if [[ $? -ne 0 ]]; then
	    echo "FATAL ERROR: Setup failed."
	    exit 1
	fi

	cd tests/
	./make.sh
	if [[ $? -ne 0 ]]; then
	    echo "FATAL ERROR: Test failed."
	    exit 1
	fi
	cd ..

	echo "Copying..."

	# Copy all necessary files into build/
	cp *.ino                                   build/build.ino
	cp *.h                                     build/
	cp libraries/bite/bite.h                   build/

	echo "Compiling..."

	# Goto build directory with all generated files
	cd build

	echo "PROPS: " ${PROPS}
	echo "FQBN: " ${FQBN}
	while ! ${COMPILER} compile -b ${BOARD}${FQBN} --warnings "all" \
			   ${PROPS} -e --libraries "libraries/" \
			   ${EXTRA_FLAGS}; do
		read -p "Press any key to continue "
		exit
	done
}

# Function to monitor
monitor() {
	if [ -n "${SERIAL_PORT+x}" ]; then
		while true; do
			${COMPILER} monitor -p ${SERIAL_PORT} \
			      --config baudrate=${MONITOR_BAUD} ${EXTRA_FLAGS};
			sleep 1
		done
	fi
}

upload() {
	if [ -n "${SERIAL_PORT+x}" ]; then
		while ! ${COMPILER} upload -b ${BOARD}${FQBN} \
				   -p ${SERIAL_PORT} ${EXTRA_FLAGS}; do
			sleep 1
		done
	fi
}

web_upload() {
	local BOARD_PATH="${BOARD//:/\.}"  # Replace ':' with '.'
	local FILE="build/${BOARD_PATH}/$(basename "$PWD").ino.bin"

	if [ ! -f "$FILE" ]; then
		echo "Firmware file not found: $FILE"
		return 1
	fi

	while true; do
		echo "Uploading $FILE to http://${OTA_IP}/update..."
		curl -F "file=@$FILE" http://${OTA_IP}/update && break
		echo "Upload failed, retrying in 1 second..."
		sleep 1
	done

	echo "Upload complete."
}

# Check if the argument is "monitor"
if [ "$1" == "monitor" ]; then
	monitor
elif [ "$1" == "upload" ]; then
	cd build
	upload
elif [ "$1" == "flash" ]; then
	cd build
	upload
elif [[ "$1" == "web" && ( "$2" == "upload" || "$2" == "flash" ) ]]; then
	cd build
	web_upload
elif [ "$1" == "web" ]; then
	compile
	web_upload
else
	compile
	upload
	monitor
fi

read -p "Press any key to continue "
