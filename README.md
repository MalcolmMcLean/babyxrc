# babyxrc

 <IMG src = "http://malcolmmclean.github.io/babyxrc/babyxrc_logo.svg" alt="Baby X Resource compiler" width = "32" height = "32"> </IMG> This is the Baby X resource compiler. It's intended for use with Baby X, but can be used with any programs, it's a standalone system.

The purpose of the program is to convert resources - images, text, fonts and so on - to compilable ANSI C. It's not meant to be a fully-fledged image processing program, but it allows for resizing and expansion of images. It also rasterises fonts, so you can take font processing out of programs - the font is dumped in a simple raster graphic format. You are of course then restricted to one size of font.

Check out our [webpages](http://malcolmmclean.github.io/babyxrc).

And the new BabyXFS (Baby X FileSystem) project [webpages](http://malcolmmclean.github.io/babyxrc/BabyXFS.html).

### Usage

```
babyxrc [options] <script.xml>

Options:
      -header - output a .h header file instead of a .c source file.
 
```

### Building
There is a CMake script. So if you have CMake, make a directory called "build" under the BabyXRC root directory containing CMakeLists.txt, navigate to it, then type

```
cmake ..
make
```
or
```
cmake -G <your generator> ..
```

If you don't have CMake, the code is all clean portable C 99 with no dependencies or problems. So
it's easy to compile on the commandline. Navigate to the "src" directory and type

```
gcc *.c freetype/*.c samplerate/*.c -lm
```
should do it. Replace "gcc" with your C compiler of choice.  

 
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

Audio support has been added. Three formats are supported, wav, 
aiff, and mp3. Only 16 bit uncompressed aiff files are currently
supported. Mp3 files are decompressed and written out as pcm samples.
(If you can play mp3 data, use the \<binary\> tag). There is an optional 
"samplerate" attribute which will resample the audio at the rate required 
by your program.

There's now much better string support. You can add a string as a C
string literal with the \<string\> tag, or you can add as UTF-8 with
the \<utf8\> tag. UTF-16 source is difficult, but the Baby X resource
compiler will attempt to intelligently recognise UTF-16. There's also
brand new support for internationalization. Whilst there is a very strong
case for allowing UTF-16 input, allowing UTF-16 output was a more difficult
decision. UTF-16 encoding should be discouraged. However sometimes people
might find it easier to work with UTF-16, so there is now a \<utf16\> tag.
Note that it won't handle surrogate pairs correctly unless you set the
allowsurrogatepairs attribute. The main reason for using UTF-16 is to
guarantee one symbol per character, so surrogate pairs are disallowed by
default. 

There's an experimental \<dataframe\> tag which allows you to import CSV
data. It is then written out as an array of C structs, with the fields
determined by the header. This might or might not work out in actual use.

Sometimes the small things make all the difference. There is now a \<comment\>
tag which inserts a comment into the output. It's vital for attaching 
licence data to open source resources that might be GPLed. 

Often you want add an entire directory. This cannot be achieved portably 
because there is no way to traverse a directory tree in ANSI C. However there 
is now a solution which uses the supplementary program babyxfs_dirtoxml. It's 
documented [here](http://malcolmmclean.github.io/babyxrc/importingdirectories.html).

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

 




  
