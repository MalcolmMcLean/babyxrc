The Baby X FileSystem Program project

These files are source for the Baby X file system project, a suite of ancillary code to babyxrc, the Baby X resource compiler. The programs are designed for use with Baby X and the resource compiler.

The problem we are trying to solve is mounting directories. Ideally you would have a <directory> tag in the resource compiler, and mount a directory. Unfortunately this cannot be achieved whist keeping the resource compiler portable, because there is no way of traversing a directory in ANSI C. And there are good reasons for that, whilst it is unproblematic on a single user desktop system, it heavy multi user environments there can be severe problems when users traverse directories.

So the approach taken has been to keep the directory traversal outside of the resource compiler, and move it into the babyxfs_ Baby X filesystem programs. These write directories in a simple xml format with a <FileSystem> tag at root and two tags, <directory> and <file> to indicate the directories. The xml files can then be incorporated into C programs as strings, using the resource compiler.

Then you will want to manipulate the strings easily in your own programs, and so code is provided here to achieve that.

asciitostring.c - routines to escape strings to C strings.
xmlparser2.c - a very powerful but baby XML parser, in the spirit of Baby X.
bbx_filesystem.c - mount a FileSystem xml as a directory in user programs.

bbx_writesource.c - code so you can write yur own source code to disk.
bbx_writesource_flat.c - ANSI compliant function which doesn't create directories.

babyxfs_dirtoxml.c - babyxfs_dirtoxml main file.
babyxfs_xmltodir.c - babyxfs_xmltodir main file.
babyxfs_ls.c - babyxfs_ls main file.
testbabyxfilesystem.c - testbabyxfilesystem main file. 

source.c - test data to pass to bbx_writesource.c.

The programs

babyxfs_dirroxml

Converts a directory to an xml file by crawling it recursively. 

babyxfs_xmltodir 

Converts an xml file to a directory.

babyxfs_ls 

Lists the files is a FileSystem xml file using globs (wildcards, eg "*.c")   

testbabyxfilessystem 

Test and demonstration program to show the Baby X file system BBX_FileSystem off.


Have fun and take care.

And as always, Baby X is free to anyone for any use.

Malcolm McLean 
  