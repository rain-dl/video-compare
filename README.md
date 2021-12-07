video-compare
=============

Split screen video comparison tool written in C++14 using FFmpeg libraries and SDL2. 

`video-compare` can be used to visually compare e.g. the effect of codecs and resizing algorithms on
two video files played in sync. The tool is not very restrictive as videos are not required to be the
same resolution, color format, container format, codec or duration. However, for the best result video
files should have the same frame rate.

A movable slider enables easy viewing of the difference across any region of interest.

Thanks to the versatility of FFmpeg, it is actually also possible to use `video-compare` to compare
two images. The common PNG and JPEG formats have been successfully tested to work.

Screenshots
-----------

Visual compare mode:
![Visual compare mode](screenshot_1.jpg?raw=true)

Subtraction mode (and zoom activated):
![Subtraction mode"](screenshot_2.jpg?raw=true)

Credits
-------

`video-compare` was created by Jon Frydensbjerg (email: jon@pixop.com). The code is mainly based on
the excellent video player GitHub project: https://github.com/pockethook/player

Many thanks to the FFmpeg and SDL2 authors.

Usage
-----

Launch in disallow high DPI mode. Video pixels become doubled on high DPI displays. Recommended
for displaying HD 1080p video on e.g. a Retina 5K display:

    ./video-compare video1.mp4 video2.mp4

Controls
--------

* Space: Toggle play/pause
* Escape: Quit
* Down arrow: Seek 15 seconds backward
* Left arrow: Seek 1 second backward
* Page down: Seek 600 seconds backward
* Up arrow: Seek 15 seconds forward
* Right arrow: Seek 1 second forward
* Page up: Seek 600 seconds forward
* S: Swap left and right video
* A: Previous frame
* D: Next frame
* 1: Toggle hide/show left video
* 2: Toggle hide/show right video
* 3: Toggle hide/show HUD
* 0: Toggle video/subtraction mode
* +/Wheel Up: Zoom in
* -/Wheel Down: Zoom out
* Keypad Up arrow: Move window up
* Keypad Down arrow: Move window down
* Keypad Left arrow: Move window left
* Keypad Right arrow: Move window right
* Left Button Down: Move slider position
* Right Button Down: Move window
* Ctrl + Left Button Click: Seek based on the horizontal position of the window width

Requirements
------------

Requires FFmpeg headers and development libraries to be installed, along with SDL2
and its TrueType font rendering add on (libsdl2_ttf).

Build
-----

    make

Notes
-----

1. Audio playback is not supported.

2. The code is hard to maintain at the moment (too many copy/paste and lazy solutions).
My intention has mainly been to build a tool in a few days which gets the job done. 
A lot of refactoring and clean-up is required when I have the time for it. Consider
yourself warned! ;-)

3. Pre-built Windows 64-bit releases are available from this page (simply extract the 
.zip-archive and run `video-compare.exe` from a command prompt).
