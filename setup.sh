#!/bin/bash
set -x

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

function git_simple_clone() {
	REPO="$1"
	VERSION="$2"
	DIR=$(basename "$REPO" .git)

	if cd "$DIR"; then
		git pull --ff-only
	else
		git clone "$REPO"
		cd "$DIR"
	fi

	git checkout "$VERSION"
	cd ..
}

#SETUP Clone git libraries
mkdir -p libraries
cd libraries
echo "Cloning git libraries..."

git_simple_clone "https://github.com/bblanchon/ArduinoJson.git"
git_simple_clone "https://github.com/adafruit/Adafruit_NeoPixel.git"
git_simple_clone "https://github.com/ESP32Async/AsyncTCP.git"
git_simple_clone "https://github.com/ESP32Async/ESPAsyncWebServer.git"
git_simple_clone "https://github.com/furdog/bitE.git" b6d97b4341532477cfa5ea06db6dd3811e19d9b8
git_simple_clone 'https://github.com/furdog/charge_counter.git' v0.1.0
git_simple_clone 'https://github.com/furdog/iso_tp.git' d570deb25a2eaa2edb146ccb69eab4c2b8b64592

cd ..
