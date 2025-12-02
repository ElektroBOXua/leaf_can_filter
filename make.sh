#!/bin/bash

###############################################################################
# CONFIGURATION:
###############################################################################
COMPILER="$PWD"/tools/arduino-cli

SERIAL_PORT=COM78
MONITOR_BAUD=115200
export OTA_IP="7.7.7.7"

export TARGET_NAME="LeafBOX"
export TARGET=leaf_can_filter_esp32c6_hw1
#export TARGET=leaf_can_filter_esp32c6_hw1_zero

#EXTRA_FLAGS="-v"

###############################################################################
# TARGETS AVAILABLE
###############################################################################

if [ "$TARGET" == "leaf_can_filter_esp32c6_hw1" ]; then
	BOARD=esp32:esp32:esp32c6
	FQBN=:CDCOnBoot=cdc
	echo "#define CAN_FILTER_ESP32C6_SUPER_MINI" > target.gen.h
elif [ "$TARGET" == "leaf_can_filter_esp32c6_hw1_zero" ]; then
	BOARD=esp32:esp32:esp32c6
	FQBN=:CDCOnBoot=cdc
	echo "#define CAN_FILTER_ESP32C6_ZERO" > target.gen.h
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

mk_base64_updater()
{
	local BOARD_PATH="${BOARD//:/\.}"  # Replace ':' with '.'
	local FILE="build/${BOARD_PATH}/$(basename "$PWD").ino.bin"

	# Generate BASE64 firmware
	export BASE64_ENCODED_FIRMWARE=$(base64 -w 0 "$FILE")

	# Generate updater html file (with embedded base64 firmware)
	../awk/ENV.awk ../web/updater.html > \
		     build/"$BOARD_PATH"/"$TARGET"_"$GIT_REPO_VERSION".html
}

compile() {
	# enumerate (and empty) all generated files
	mkdir -p build

	rm build/* 2> /dev/null # Clean build
	

	echo "Initial setup..."

	# Setup tools and libraries
	./setup.sh
	if [[ $? -ne 0 ]]; then
	    echo "FATAL ERROR: Setup failed."
	    exit 1
	fi


	echo "Testing..."
	
	cd tests/
	./make.sh
	if [[ $? -ne 0 ]]; then
	    echo "FATAL ERROR: Test failed."
	    exit 1
	fi
	cd ..
	pushd libraries/charge_counter/
	./make.sh
	popd


	echo "Copying..."

	# Copy all necessary files into build/
	cp hal/*.ino                                 build/build.ino
	cp hal/*.h                                   build/
	cp *.h                                       build/
	cp libraries/bite/bite.h                     build/
	cp libraries/charge_counter/charge_counter.h build/

	echo "Building web..."

	# Replace environment variables inside web page
	awk/ENV.awk web/index.html >> build/index.html

	# Compress WEB
	gzip -9 -c build/index.html >> build/index.html.gz


	echo "Compiling..."

	# Goto build directory with all generated files
	cd build

	# Make C array from WEB
	echo "#pragma once" >> index.h
	xxd -i index.html.gz >> index.h

	echo "PROPS: " ${PROPS}
	echo "FQBN: " ${FQBN}
	while ! ${COMPILER} compile -b ${BOARD}${FQBN} --warnings "all" \
			   ${PROPS} -e --libraries "../libraries/" \
			   ${EXTRA_FLAGS}; do
		read -p "Press any key to continue "
		exit
	done

	mk_base64_updater
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
