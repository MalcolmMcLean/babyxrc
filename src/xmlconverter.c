//
//  xmlconverter.c
//  babyxrc
//
//  Created by Malcolm McLean on 24/05/2024.
//

#include "xmlconverter.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int cJSONToXML_r(FILE *fp, cJSON *json, int depth);
static int iselementchar(int ch);
static char *xml_escape(const char *data);

int xml_fromCSV(FILE *fp, CSV *csv)
{
    int width, height;
    int i, ii;
    const char *fieldname;
    char *elementname;
    char buff[256];
    int type;
    const char *str;
    char *xmltext;
    
    csv_getsize(csv, &width, &height);
    
    fprintf(fp, "<CSV>\n");
    
    for (i = 0; i < height; i++)
    {
        fprintf(fp, "\t<Row>\n");
        for (ii = 0; ii < width; ii++)
        {
            fieldname = csv_column(csv, ii, &type);
            if (!fieldname)
            {
                snprintf(buff, 256, "Col%d", ii +1);
                fieldname = buff;
            }
            elementname = xml_makeelementname(fieldname);
            fprintf(fp, "\t\t<%s>", elementname);
         
            
            if (csv_hasdata(csv, ii, i))
            {
                if (type == CSV_STRING)
                {
                    str = csv_getstr(csv, ii, i);
                    if (str)
                    {
                        xmltext = xml_escape(str);
                        fprintf(fp, "%s", xmltext);
                        free(xmltext);
                    }
                }
                else if (type == CSV_REAL)
                    fprintf(fp, "%g", csv_get(csv, ii, i));
                else if (type == CSV_BOOL)
                    fprintf(fp, "%s", csv_get(csv, ii, i) == 0.0 ? "false" : "true");
            }
            fprintf(fp,"</%s>\n", elementname);
            free(elementname);
            elementname = 0;
        }
        fprintf(fp, "\t</Row>\n");
    }
    
    fprintf(fp, "</CSV>\n");
    
    return  0;
}

int xml_fromJSON(FILE *fp, cJSON *json)
{
    return cJSONToXML_r(fp, json, 0);
}

static int cJSONToXML_r(FILE *fp, cJSON *json, int depth)
{
    int i;
    int N;
    const char *tag;
    const char *str;
    char *xmltext;
    
    while (json)
    {
        for (i = 0; i < depth; i++)
            fprintf(fp, "\t");
        tag = json->string;
        if (!tag)
            tag = "Row";
        fprintf(fp, "<%s>", tag);
        if (json->child)
            fprintf(fp, "\n");
        if (cJSON_IsString(json))
        {
            str = cJSON_GetStringValue(json);
            if (str)
            {
                xmltext = xml_escape(str);
                fprintf(fp, "%s", xmltext);
                free(xmltext);
            }
        }
        else if (cJSON_IsNumber(json))
            fprintf(fp, "%g", cJSON_GetNumberValue(json));
        else if (cJSON_IsBool(json))
            fprintf(fp,"%s", cJSON_IsTrue(json) ? "true" : "false");
        else if (cJSON_IsArray(json))
        {
            N = cJSON_GetArraySize(json);
            for (i = 0; i < N; i++)
            {
                cJSONToXML_r(fp, cJSON_GetArrayItem(json, i), depth +1);
            }
        }
        if (json->child)
            cJSONToXML_r(fp, json->child, depth +1);
        
        if (json->child)
        {
            for (i = 0; i < depth; i++)
                fprintf(fp, "\t");
        }
        fprintf(fp, "</%s>\n", tag);
        
        json = json->next;
    }
    
    return 0;
}

char *xml_makeelementname(const char *str)
{
    char *answer;
    int i;
    int j = 0;
    
    answer = malloc(strlen(str) + 2);
    if (!answer)
        return 0;
    
    if (!isalpha(str[0]) && str[0] != '_')
    {
        answer[0] = '_';
        j = 1;
    }
    for (i = 0; str[i]; i++)
    {
        if (!iselementchar(str[i]))
            answer[j++] = '_';
        else
            answer[j++] = str[i];
    }
    answer[j++] = 0;
    
    return answer;
}

static int iselementchar(int ch)
{
    if (isalnum(ch))
        return 1;
    if (ch == '-'|| ch == '_' || ch == '.')
        return 1;
    return 0;
}

static char *xml_escape(const char *data)
{
    int i;
    int size = 0;
    char * answer;
    char *ptr;
    
    for (i = 0; data[i]; i++)
    {
        switch(data[i]) {
            case '&':  size += 5; break;
            case '\"': size += 6; break;
            case '\'': size += 6; break;
            case '<':  size += 4; break;
            case '>':  size += 4; break;
            default:   size += 1; break;
        }
    }
    answer = malloc(size+1);
    if (!answer)
        goto out_of_memory;
    
    ptr = answer;
    for (i = 0; data[i]; i++) {
        switch(data[i]) {
            case '&':  strcpy(ptr, "&amp;"); ptr += 5;     break;
            case '\"': strcpy(ptr, "&quot;"); ptr += 6;    break;
            case '\'': strcpy(ptr, "&apos;"); ptr += 6;     break;
            case '<':  strcpy(ptr, "&lt;"); ptr += 4;      break;
            case '>':  strcpy(ptr, "&gt;"); ptr += 4;      break;
            default:   *ptr++ = data[i]; break;
        }
    }
    *ptr++ = 0;
    
    return answer;
out_of_memory:
    return 0;
}
