# babyxrc

This is the Baby X resource compiler. It's intended for use with Baby X, but can be used with any programs, it's a standalone system.

The purpose of the program is to convert resources - images, text, fonts and so on - to compilable ANSI C. It's not meant to be a fully-fledged image processing program, but it allows for resizing and expansion of images. It also rasterises fonts, so you can take font processing out of programs - the font is dumped in a simple raster graphic format. You are of course then restricted to one size of font.

 
Input file format
-----------------

Example script file:
~~~
<BabyXRC>
<image src = "smiley.png", name = "fred", width = "10", height = "10"> </image>
<image src = "lena.jpg"> </image>
<font src = "arial.ttf", name = "arial12", points="12"> </font>
<string src = "external.txt"> </string>
<string name = "internal"> "A C string\n"> </string>
<string name = "embedded"> Embedded string </string>
<audio src = "Beep.wav", name = "beep", samplerate = "44100"> </audio> 
<binary src = "dump.bin", name = "dump_bin"> </binary>
</BabyXRC>
~~~


### Scope and use

The idea is to act as a portable resource compiler of the type that
comes attached to monolithic IDEs. The only way of importing binary
data portably into C is to dump as C source files. Fortunately these
zip through modern compilers pretty quickly, there's no complexity
or optimisation. However you can't edit dumped source directly, at
least easily. So you need to keep a list of original binary resources
handy. A simple xml file lists the resources to compile.

Because resizing images is such a frequent requirement, it is built
in. Current just two algorithms, both of which operate in rgba
space, an averaging "shrink" and bilinear interpolation for expansion.
The resource compiler isn't going to support a full image processing
transform chain, however, at least yet.

The other major functionality is font rasterisation. The freetype
library is included and used to rasterise the font. However freetype
is slow and complicated for runtime, and many programs just use a 
restricted set of fonts at preset sizes. So they are dumped in a 
simple raster format, and it is then trivial to write a "draw text"
routine.

Audio support has been added. Currently only wav files are supported. 
There is an optional "samplerate" attribute which will resample the 
audio at the rate required by your program.

Currently I use the resource compiler myself, and the intention is
to gradually roll it out as a standalone tool for general users.
Those users will be C programmers, so the tool can be modified to
produce slightly different output. If you need ABGR rather than
RGBA, for example. A savejpeg function that is not actually linked
has been included, you can use it save images in compressed 
format if that's what you want.

### Other uses

The codebase is also by its nature a treasury of useful routines,
some of them written by me, some by other people. There's a
loadimage function which will load practically anything - TIFFs
in virtually any format, JPEGs, PNGs, SVGs, GIF, BMP. You're welcome
to raid the components for your own purposes. There's also a vanilla 
xml parser, a commandline options parser, and the image resizing
routines. Do respect the licence terms of the components not
authored by me.

The code is meant to be portable ANSI C with no dependencies except
the standard library. 

 




  
