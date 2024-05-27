//
//  dataframes.h
//  babyxrc
//
//  Created by Malcolm McLean on 24/05/2024.
//

#ifndef dataframes_h
#define dataframes_h

#include "xmlparser2.h"
#include <stdio.h>

int processdataframenode(FILE *fp, XMLNODE *node, int header, void *xformcontext);

#endif /* dataframes_h */
