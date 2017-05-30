# Description
... to follow ...

# How to build
## Install required packages
~~~~
sudo apt-get install git pkg-config build-essential libasound2-dev libfreetype6-dev libcurl4-gnutls-dev libx11-dev libxext-dev libxinerama-dev libglib2.0-dev mesa-common-dev libxrandr-dev libxcursor-dev libgtk2.0-dev libcairo2-dev libwebkitgtk-3.0-dev libwebkit2gtk-4.0-dev libgl1-mesa-dev
~~~~

## Get JUCE
kVstSequencer is made with the [JUCE](https://www.juce.com/ "JUCE") framework, which needs to be cloned as follows:

~~~~
cd ~
mkdir -p git/WeAreROLI/
cd git/WeAreROLI/
git clone https://github.com/WeAreROLI/JUCE.git
~~~~

## Check out libkohaerenzstiftung and kVstSequencer itself
kVstSequencer also relies on libkohaerenzstiftung, my tiny little utility library with defines and functions I commonly use across my different projects. You can either clone my whole C repository or just sparse-check out kVstSequencer and libkohaerenzstiftung:

~~~~
cd ~
mkdir -p git/kohaerenzstifter/c
cd git/kohaerenzstifter/c
git init
git remote add -f origin https://github.com/kohaerenzstifter/c.git
echo "kohaerenzstiftung" >> .git/info/sparse-checkout
echo "kVstSequencer" >> .git/info/sparse-checkout
git pull origin master
~~~~

## Build kVstSequencer

~~~~
cd kVstSequencer/Builds/LinuxMakefile/
make KOHAERENZSTIFTUNG_PATH=~/git/kohaerenzstifter/c/kohaerenzstiftung/ JUCE_PATH=~/git/WeAreROLI/JUCE/
~~~~

The "build" subdirectory should now contain the plugin named "kVstSequencer.so".
