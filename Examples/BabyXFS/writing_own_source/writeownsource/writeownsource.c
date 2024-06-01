//
//  writeownsource.c
//  babyxrc
//
//  Created by Malcolm McLean on 01/06/2024.
//

/*
  program to write its own source to disk, including a documention folder with an image.
 
   
 How was this achieved? With the Baby X resource compiler and suite of helper programs to convert directories to xml.
 
 We replace the file source. c with a short placeholder.
 
 Then we convert the tree to xml.
 
 Then we run the xml throught the Baby X resource compiler to create a string with the name "source".
 Then we replace the placeholder with the real file "source.c", and recompile.
 
 The result is a program which writes it's own source and any binaries etc to disk. Very easy to do, with the power of the Baby X resource compiler.
 
 Check out the Baby X project on github.
 
 */

#include <stdio.h>
#include <stdlib.h>

#include "bbx_write_source.h"

void usage()
{
    fprintf(stderr, "writeownsource - writes its own source to disk.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: writeownsource <destination folder>\n");
    
    exit(EXIT_FAILURE);
}

extern char source[];

int main(int argc, char **argv)
{
    int err;
    
    if (argc != 2)
        usage();
        
    err = bbx_write_source(source, argv[1], "source.c", "source");
    if (err)
        fprintf(stderr, "error writing source\n");
    
    exit(err ? EXIT_FAILURE : 0);
}

