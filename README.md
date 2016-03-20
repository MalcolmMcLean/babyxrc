# babyxrc

This is the Baby X resource compiler. It's intended for use with BabyX, but can be used with any programs, it's a standalone system.

The purpose of the program is to convert resources - images, text, fonts and so on - to compileable ANSI C. It's not meant to be a fully-fledged image processing progrm, but it allows for resizing and expansion of images. It also rasterises fonts, so you can take font processing out of programs - the font is dumped ina simple raster graphic format. You are of course then restricted to one size of font.

 
Input file format
-----------------

Example script file:
<BabyXRC>
<image src = "smiley.png", name = "fred", width = "10", height = "10"> </image>
<image src = "lena.jpg"> </image>
<font src = "arial.ttf", name = "arial12", points="12"> </font>
<string src = "external.txt"> </string>
<string name = "internal"> "A C string\n"> </string>
<string name = "embedded"> Embedded string </string>
<binary src = "dump.bin", name = "dump_bin"> </binary>
</BabyXRC>
