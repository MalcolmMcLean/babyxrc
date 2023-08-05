Here are some example files to show how to use the
resource compiler, also test it.

Call like this:

BabyXRC script.xml > outputfile.c
BabyXRC -header script.xml > outputfile.h

AminoInvaders is a little fun biochemistry game project
that includes the amino acids. Amino acid gif files were
nicked from the web. They are too large for the game, so
they are resized slightly. Then dumped as 32 bit rgbas.

I've also added a cursor and an SVG file to demonstrate
the system.

AudioTest tests the Baby X resource compiler's audio file
capabilities. Audio data can get very large, so the full
output file is not included, even though the test samples are
very short.

Fonts tests the font ripper. You can render fonts wth freetype, 
but in a small program this is both slow and overkill. If
you only need fonts in two or three known types and sizes,
it's easier to pre-render them and then simply write them to
the display. 
  
Dataframes test the ability to load tabular data from an
external source. Data is provided in the form of .csv files,
and written out as C structs or matrices.

Text tests the Baby X resource compiler's text capabiliites.
We've added international support, so there's now an
<international> tag that takes a list of strings or unicode
data with a "language" attribute. It then generates a simple 
runtime function to select the string ofthe right language.

Whilst I strongly encourage everyone to store their data as UTF-8,
the uft8 tag will attempt to smart detect UTF-16 files, and
converts them to utf-8 before writing them out.

The text tags are

<string> output data as a human-raadable C string.
<utf8> output data as a binary dump of UTF-8.
<utf16> output data as an array of unsigned shorts in UTF-16.
<international> output data in several languages.

The <string> tag should take ASCII, though it will accept non-ASCII 
characters. Quote the string to avoid it being escaped. Use the <utf8>
tag if you need to enter a string literal which is quoted.

The <utf8> tag is the one you should use for string data which might
contain non-ASCII characters. 

The <utf16> tag is provided for compatibility with simple programs.
Generally UTF-16 is discouraged and you should pass string data about
as UTF-8, converting to another representation only immediately before
calling an output function. Usually the reason for using UTF-16 is that
the program wants one wide character to equal one glyph. So surrogate
pairs are by default turned off. If you want surrogate pairs, pass the
boolean option "allowsurrogatepairs" and set to "true" or "1">

The <international> tag is a wrapper for a list of <string> or <utf8>
tags with the "language" attribute set. In then outputs a simple 
executable function to take a string argument to specify the language,
and return the right string. 
