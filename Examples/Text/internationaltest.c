#include <stdio.h>
#include <string.h>
char *hello_english = "Hello World";
char *hello_latin = "Save munde";
char *hello_french = "Bonjour tout le monde";
const char *get_hello(const char *language)
{
    if (!strcmp("english", language))
        return hello_english;
    if (!strcmp("latin", language))
        return hello_latin;
    if (!strcmp("french", language))
        return hello_french;
    return 0;
}
char helloutf8_english[] = {
0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72, 0x6c, 
0x64, 0x00
};
char helloutf8_latin[] = {
0x53, 0x61, 0x76, 0x65, 0x20, 0x6d, 0x75, 0x6e, 0x64, 0x65, 
0x00
};
char helloutf8_french[] = {
0x42, 0x6f, 0x6e, 0x6a, 0x6f, 0x75, 0x72, 0x20, 0x74, 0x6f, 
0x75, 0x74, 0x20, 0x6c, 0x65, 0x20, 0x6d, 0x6f, 0x6e, 0x64, 
0x65, 0x00
};
char helloutf8_greek[] = {
0xce, 0x93, 0xce, 0xb5, 0xce, 0xb9, 0xce, 0xac, 0x20, 0xcf, 
0x83, 0xce, 0xbf, 0xcf, 0x85, 0x20, 0xce, 0x9a, 0xcf, 0x8c, 
0xcf, 0x83, 0xce, 0xbc, 0xce, 0xb5, 0x00
};
const char *get_helloutf8(const char *language)
{
    if (!strcmp("english", language))
        return helloutf8_english;
    if (!strcmp("latin", language))
        return helloutf8_latin;
    if (!strcmp("french", language))
        return helloutf8_french;
    if (!strcmp("greek", language))
        return helloutf8_greek;
    return 0;
}

int main(void)
{
  printf("English %s\n", get_hello("english"));
  printf("Latin %s\n", get_hello("latin"));
  printf("French %s\n", get_hello("french"));

  printf("English %s\n", get_helloutf8("english"));
  printf("Latin %s\n", get_helloutf8("latin"));
  printf("French %s\n", get_helloutf8("french"));
  printf("Greek %s\n", get_helloutf8("greek"));

  return 0;
}
