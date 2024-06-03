//
//  babyxfs_test.c
//  babyxfs
//
//  Created by Malcolm McLean on 28/05/2024.
//
#include "xmlparser2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bbx_write_source_archive.h"

/*
    This program is an impementation of the rm command for FileSystem.xml files.
 
     We generate an XML file which represents a filesystem. Then we query it for files.
     Unfortunately there is no way to traverse a physical computer's filesystem with
     complete portablity, and so the encoder, babyxfs_dirtoxml, will not run everywhere.
     But the decoder is completetely portable. And you can incorporate it into your own
     programs to have embedded files. Use the Baby X resource compiler to convert the XML
     to a string, then load it using xmldocfromstring.
 
     The XML is very simple
     
     <FileSystem>
           <directory name="poems">
               <directory name="Shakespeare">
                    <file name="Sonnet 12" type="text">
                        When do I count the clock that tells the time?
                    </file>
                </directory>
                <directory name="Blake">
                    <file name="Tyger" type="text">
                            Tyger, tyger, burning bright,
                            Through the forests of the night,
                    </file>
                    <file name="BlakePicture.png" type="binary">
 <![CDATA[M)"E3'U@":H````0#)A$12!``#H`````6(8````056"`0``@"HFV0#!52#-$(
 M0)W;FE&;E!``(E8E7>`43EN%'_[>3/D`$2("(E0-4$D.!0*A>H((=04)D$(A
 M2,$(HBHB(+N"L60$1PR*ZJBH@K*UU*6P"+*6PN;06$1==Q"V0EW-P08W]-OW
 ]]>
                    </file>
                </directory>
           </directory>
     </FilesSystem>
 
     All the code was written by Malcolm  McLean.
      It is free for any use.
 */

/*
   unlink a node from the document tree
 
   returns 0 on success, -1 on fail
 */
int xml_node_unlink(XMLNODE *root, XMLNODE *target)
{
    XMLNODE *sib;
    XMLNODE *prev;
    int answer = -1;
    
    while (root)
    {
        if (root->child)
        {
            prev = root->child;
            sib = prev->next;
            
            if (prev == target)
            {
                root->child = sib;
                prev->next = 0;
                return 0;
            }
            
            while (sib)
            {
                if (sib == target)
                {
                    prev->next = sib->next;
                    sib->next = 0;
                    return 0;
                }
            
                prev = prev->next;
                sib = sib->next;
            }
            answer = xml_node_unlink(root->child, target);
            if (!answer)
                return 0;
        }
        
        root = root->next;
    }
    
    return - 1;
}

/*
  Find a node from a path.
 
  pos indicates the position along the path to start from.
 */
XMLNODE *findnodebypath(XMLNODE *node, const char *path, int pos)
{
    XMLNODE *answer = 0;
    const char *name = 0;
    int i;
    
   while (node)
   {
      
       if (!strcmp(xml_gettag(node), "file"))
       {
           name = xml_getattribute(node, "name");
           if (!name)
               goto nextnode;
           if (!strcmp(name, path + pos))
               return node;
       }
       else if (!strcmp(xml_gettag(node), "directory"))
       {
           name = xml_getattribute(node, "name");
           if (!name)
               continue;
           for (i = 0; name[i] && path[pos+i]; i++)
           {
               if (name[i] != path[pos+i])
               {
                   goto nextnode;
               }
           }
           
           if (name[i] == 0 && path[pos + i] == 0)
               return node;
           if (name[i] == 0 && path[pos + i] == '/')
               return findnodebypath(node->child, path, pos + i + 1);
       }
       else if (!strcmp(xml_gettag(node), "FileSystem"))
       {
           if (path[pos] != '/')
               return 0;
           node = node->child;
           while (node)
           {
               answer = findnodebypath(node, path, pos + 1);
               if (answer)
                   return answer;
               node = node->next;
           }
           return 0;
       }
       
   nextnode:
       node = node->next;
   }
    
    return  0;
}

/*
  Get the node with the tag "FileSystem"
 */
XMLNODE *bbx_fs_getfilesystemroot(XMLNODE *root)
{
    XMLNODE *answer;
    
    answer = root;
    while (root)
    {
        if (!strcmp(xml_gettag(root), "FileSystem"))
            return root;
        root = root->next;
    }
    
    root = answer;
    while (root)
    {
        answer = bbx_fs_getfilesystemroot(root->child);
        if (answer)
            return answer;
        root = root->next;
    }
    
    return 0;
}

/*
   remove a file from a node
 */
int babyxfs_rm(XMLNODE *root, const char *path)
{
    XMLNODE *node;
    
    node = findnodebypath(root, path, 0);
    if (!node)
    {
        fprintf(stderr, "Can't find file\n");
        return -1;
    }
    if (node->child)
    {
        fprintf(stderr, "Can't delete a non-empty directory\n");
        return -1;
    }
    xml_node_unlink(root, node);
    
    return 0;
}


void usage()
{
    fprintf(stderr, "babyxfs_rm: remove a file from a FileSystem XML archive\n");
    fprintf(stderr, "Usage: - babyxfs_rm <filesystem.xml> <pathtofile>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Generate the FileSystem files with the program babyxfs_dirtoxml\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "By Malcolm McLean\n");
    fprintf(stderr, "Part of the BabyX project.\n");
    fprintf(stderr, "Check us out on github and get involved.\n");
    fprintf(stderr, "Program and source free to anyone for any use.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    XMLDOC *doc = 0;
    char error[1024];
    int err;
    XMLNODE *fs_root;
    FILE *fp;
    
    char **list;
    int i;
    
    if (argc != 3)
        usage();
    
    doc = loadxmldoc(argv[1], error, 1024);
    if (!doc)
    {
        fprintf(stderr, "%s\n", error);
        return -1;
    }
    
    fs_root = bbx_fs_getfilesystemroot(xml_getroot(doc));
    babyxfs_rm(fs_root, argv[2]);
    
    fp = fopen(argv[1], "w");
    if (!fp)
    {
        fprintf(stderr, "babxfs_rm: can't write target directory\n");
        goto error_exit;
    }
    
    err = bbx_write_source_archive_root(fp, xml_getroot(doc), 0, "placholder", "..", "catastrope");
    if (err)
        goto error_exit;
    fclose (fp);
    fp = 0;
    
    killxmldoc(doc);
    return 0;
    
error_exit:
    fclose(fp);
    killxmldoc(doc);
    exit(EXIT_FAILURE);
}

