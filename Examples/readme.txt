Here are some example files to show how to use the
resource compiler, also test it.

Call like this:

BabyXRC script.xml > outputfile.c

AminoInvaders is a little fun biochemistry game project
that includes the amino acids. Amino acid gif files were
nicked from the web. They are too large for the game, so
they are resized slightly. Then dumped as 32 bit rgbas.

I've also added a cursor and an SVG file to demonstrate
the system.

AudioTest tests the Baby X resource compiler's audio file
capabilities. Audio data can get very large, so the full
output file is not included, even thoughthe test samples are
very short.
  
Text tests the Baby X resource compiler's text capabiliites.
We've added international support, so there's now an
<international> tag that takes a list of strings or unicode
data with a "language" attribute. It then generates a simple 
runtime fucntion to select the stringof the right language.

Whilst I strongly encourage everyone to store their data as utf-8,
the uft8 tag will attempt to smart detect utf-16 files, and
converts them to utf-8 before writing them out.



