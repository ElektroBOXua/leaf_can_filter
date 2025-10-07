#!/bin/bash

#SETUP tools
if [[ ! -d "tools/" ]]; then
	mkdir -p tools
	cd tools

	curl -L https://github.com/arduino/arduino-cli/releases/download/v1.1.1/arduino-cli_1.1.1_Windows_64bit.zip > arduino-cli.zip
	unzip arduino-cli.zip arduino-cli.exe
	rm arduino-cli.zip

	./arduino-cli core install esp32:esp32 --config-file ../.cli-config.yml

	cd ..
fi

#SETUP libraries
if [[ ! -d "libraries/" ]]; then
	mkdir -p libraries
	cd libraries

	#curl -L https://github.com/adafruit/Adafruit_CAN/archive/refs/tags/0.2.1.zip > ar.zip
	#unzip ar.zip
	#rm ar.zip

	cd ..
fi

#SETUP Clone git libraries
mkdir -p libraries
cd libraries

echo "Cloning git libraries..."

REPO_URL="https://github.com/bblanchon/ArduinoJson.git"
DEST_DIR="$(basename "$REPO_URL" .git)"
echo "$REPO_URL"
if [ -d "$DEST_DIR" ]; then
	(cd "$DEST_DIR" && git pull)
else
	(git clone "$REPO_URL")
fi

REPO_URL="https://github.com/adafruit/Adafruit_NeoPixel.git"
DEST_DIR="$(basename "$REPO_URL" .git)"
echo "$REPO_URL"
if [ -d "$DEST_DIR" ]; then
	(cd "$DEST_DIR" && git pull)
else
	(git clone "$REPO_URL")
fi

REPO_URL="https://github.com/ESP32Async/AsyncTCP.git"
DEST_DIR="$(basename "$REPO_URL" .git)"
echo "$REPO_URL"
if [ -d "$DEST_DIR" ]; then
	(cd "$DEST_DIR" && git pull)
else
	(git clone "$REPO_URL")
fi

REPO_URL="https://github.com/ESP32Async/ESPAsyncWebServer.git"
DEST_DIR="$(basename "$REPO_URL" .git)"
echo "$REPO_URL"
if [ -d "$DEST_DIR" ]; then
	(cd "$DEST_DIR" && git pull)
else
	(git clone "$REPO_URL")
fi

REPO_URL="https://github.com/furdog/bitE.git"
DEST_DIR="$(basename "$REPO_URL" .git)"
echo "$REPO_URL"
if [ -d "$DEST_DIR" ]; then
	(cd "$DEST_DIR" && git pull)
else
	(git clone "$REPO_URL")
fi
pushd "$DEST_DIR" # Use certain commit
git checkout b6d97b4341532477cfa5ea06db6dd3811e19d9b8
popd

# Clone charge_counter
REPO='https://github.com/furdog/charge_counter.git'
if cd `basename $REPO .git`; then git pull; cd ..; else git clone "$REPO"; fi

cd ..
