Writing a program's own source to disk.

Very often you want to be able to write a program's own source to disk. In fact every open source program should do this. But it can be a bit of a challenge. However the Baby X Resource compiler and the BabyX FS Ancillary programs make it extremely easy.

The problem is self containment. If you package up the source, and add it to the program to print out, it thereby becomes itself part of the source of the program. And so you are not actually printing out you own source.

So it is slightly fiddly. What we do is we package up the source, and we put the data as an xml file in the c source file source.c. But at the moment, of course, we don't have this xml file. So we use a little place holder. It makes it easier if it compiles, thought this isn't essential. 

Use babyxfs_dirtoxml to package up the source into a filesystem xml file. Then use the Baby X resource compiler to covert the xml into a .c source file with a string. Then replace the file source.c with this file, and compile.

And the result is a program which writes its own source and any supporting files to disk, as many times as you want.

And every open source program should be using this system and leveraging the power of the Baby X suite of programs.


Here's the placeholder for source.c.

//
//  source.c
//  testbabyxfilesystem
//
//  Created by Malcolm McLean on 01/06/2024.
//
char source[] = "placeholder";



placeholder_source.c - placeholder source file
real_source.c - real source file.
<writeownsource> - the source directory
writeownsource.xml - writeownsource packaged into a FileSystem XML
writesource.xml - the Baby X resource compiler script.

Note that when you have the source in the program, you don't really need the rest of the resource compiler. You can mount the source as a BBX_FileSystem. Then you have all the resources. And that's how powerful the Baby X resource compiler is becoming.

Yours, Malcolm McLean.

