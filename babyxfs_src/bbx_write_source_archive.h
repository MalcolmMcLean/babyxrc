//
//  bbx_write_source_archive.h
//  babyxrc
//
//  Created by Malcolm McLean on 02/06/2024.
//

#ifndef bbx_write_source_archive_h
#define bbx_write_source_archive_h

#include "xmlparser2.h"

void bbx_fs_xml_killnode_r(XMLNODE *node);

char *bbx_writesource_archive_node_to_text(XMLNODE *node);
unsigned char *bbx_writesource_archive_node_to_binary(XMLNODE *node, int *N);

int bbx_write_source_archive_write_to_file_node(XMLNODE *node, const unsigned char *data, int N, const char *datatype);
int bbx_write_source_archive_root(FILE *fp, XMLNODE *node, int depth, const char *source_xml, const char *source_xml_file, const char *source_xml_name);
int bbx_write_source_archive(FILE *fp, const char *source_xml, const char *source_xml_file, const char *source_xml_name);

#endif /* bbx_write_source_archive_h */
