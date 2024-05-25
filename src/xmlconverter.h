//
//  xmlconverter.h
//  babyxrc
//
//  Created by Malcolm McLean on 24/05/2024.
//

#ifndef xmlconverter_h
#define xmlconverter_h

#include "xmlparser2.h"
#include "csv.h"
#include "cJSON.h"

#include <stdio.h>

int xml_fromCSV(FILE *fp, CSV *csv);
int xml_fromJSON(FILE *fp,cJSON *json);
char *xml_makeelementname(const char *str);

#endif /* xmlconverter_h */
