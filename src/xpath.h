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

XMLNODE **xml_xpath_select(XMLDOC *doc, const char *xpath, int *Nselected, char *errormessage, int Nerr);
XMLATTRIBUTE **xml_xpath_selectattributes(XMLDOC *doc, const char *xpath, char *errormessage, int Nerr);
int xml_xpath_selectsattributes(const char *xpath, char *errormessage, int Nerr);
int xml_xpath_isvalid(const char *xpath, char *errormessage, int Nerr);
char *xml_xpath_getnodepath(XMLDOC *doc, XMLNODE *node);


#endif /* xpath_h */
