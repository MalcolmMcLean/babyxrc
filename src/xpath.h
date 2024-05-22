//
//  xpath.h
//  babyxrc
//
//  Created by Malcolm McLean on 22/05/2024.
//

#ifndef xpath_h
#define xpath_h

#include <stdio.h>
#include "xmlparser2.h"

XMLDOC *xml_selectxpath(XMLDOC *doc, const char *xpath, char *errormessage, int Nerr);

#endif /* xpath_h */
