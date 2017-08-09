# Description
Remember those typical wobbling and sliding baselines you would create with the Roland TB 303 step sequencer? Remember how easy it was to create rhythms with TR 808/909? Nowadays, instead of using (expensive) real instruments, many people resort to VST synths for their sound creations. Alas, creating pattern just isn't so much fun with your DAW's composer as it used to be with the Roland step sequencers, don't you think?
As a Linux user, I soon found myself searching for (Linux-compatible) step sequencer plugins to trigger my DAW's synths - with very scarce results. So I decided to write my own, which, as I found out, could be easily done with the help of the [JUCE](https://www.juce.com/ "JUCE") framework. If this made you curious, do read on how to build and use kVstSequencer.

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

## How to use

I have tested kVstSequencer under Linux with Bitwig and will show you briefly how this is done. Please note that some of the features I describe here are NOT possible in Bitwig 1.x, but Bitwig 2.x can be downloaded and run for free in demo mode to try for yourself!
First you have to tell Bitwig where it can find the step sequencer plugin, so on the right side of the screen, make the "Files Browser" appear, next right-click on "Library Locations" and then on "Add Plug-in Location" and select the correct path (e.g. ~/git/kohaerenzstifter/c/kVstSequencer/Builds/LinuxMakefile/build):

![1](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/1.jpg)

Next, select an instrument track of your choosing and within the Device Panel click on "+". In the device selection dialog that comes up, just enter "kVst" in the search bar at the top and then select "kVstSequencer" at the right:

![2](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/2.jpg)

We're not yet done setting up, so in case the step sequencer UI immediately opens up, just close it again for now:

![3](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/3.jpg)

Next we have to set up a separate instrument track to accomodate the synth to be played by the sequencer. The quickest way to do this is by clicking straight on the "+" below the last track and directly selecting the desired synth (Polysynth):

![4](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/4.jpg)

Polysynth still needs some fine-tuning, so let's first set Filter Cutoff and Resonance to around 50 percent:

![5](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/5.jpg)

Next, on the very right, enable all three of "MONO", "ST" and "FP to be able to emulate the TB-303s Slide (or Glide?) feature:

![6](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/6.jpg)

Also, set Glide intensity to around 50 percent:

![7](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/7.jpg)

In order to emulate the Accent feature, click on one of the three stacked "+" symbols at the left and insert a so-called "Expressions" modulator:

![8](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/8.jpg)

Next, click on the velocity expression (Vel) and let it control Filter Cutoff and Output Volume:

![9](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/9.jpg)



--------------------------------------------------------------------------------------------------
Note velocity will now influence the filter cutoff and volume of your synth. This is important, because by playing notes with different velocity, the step sequencer will emulate the TB 303's accent feature.
By enabling legato, we enabled the slide feature, which the step sequencer emulates by playing two notes consecutively and releasing the first *just after* the second is pressed.

We still need to hook up a so-called Note Receiver to make sure the notes played by our sequencer are received by our synth. Click on the + symbol right in front of the synth:

![10](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/10.jpg)

Next select a Note Receiver device:

![11](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/11.jpg)

Configure your Note Receiver's source to be the track in which the step sequencer is loaded:

![12](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/12.jpg)

Now change to the step sequencer's track and open the plugin GUI:

![13](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/13.jpg)

When you add kVstSequencer to your DAW's project, you find yourself at the root level. What do I mean by that? Well, a kVstSequencer pattern can be made up of several hierarchically dependent patterns. All that will make sense to you very soon. For now and for simplicity's sake, suffice it to say that you first need to add a child to the root pattern by clicking on "Children ...":

![14](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/14.jpg)

Now click on "Add" to set up a new child pattern:

![15](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/15.jpg)

As we are going to create a baseline pattern, we need to select "NOTE" for the pattern type. Let's call it "baseline" and assign MIDI channel 1:

![16](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/16.jpg)

Now enter your newly created pattern:

![17](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/17.jpg)

To demonstrate how a pattern's configuration can be changed even during playback, let's now start the playback in Bitwig:

![18](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/18.jpg)

Now let's create two note velocities, one for normal play and one for accentuated play, by clicking on velocities:

![19](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/19.jpg)

Now add two velocites by clicking on "Add". For normal play, let's assign a velocity value of 80, and for accentuated play, let it be 127:

![20](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/20.jpg)

Next, click on "Values" and create two randomly chosen notes eligible for playback:

![21](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/21.jpg)

Your pattern is currently one bar long with one step per bar. Clicking several times on that step lets you step through the various combinations of notes and velocities:

![22](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/22.jpg)

As you probably know, the TB 303 has 16 steps per bar, so let's change that by clicking on "Steps per Bar" and select 16. The step value assigned to your single step will now be copied to all 16 steps of your bar:

![23](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/23.jpg)

Of course, this sounds boring, so let's just create a random pattern by clicking on "Randomise":

![24](https://github.com/kohaerenzstifter/c/blob/master/kVstSequencer/24.jpg)








Use the radio buttons to select the type of pattern you with to create (NOTE), give your pattern a name, and select the MIDI channel number. In some DAW's (e.g. Bitwig), the MIDI channel you select here plays no role at all, since the instruments to control is determined by how you plug your devices together.
