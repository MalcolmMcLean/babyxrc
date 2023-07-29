//
//  bbx_utf8.c
//  babyxrc
//
//  Created by Malcolm McLean on 29/07/2023.
//

#include "bbx_utf8.h"

static const unsigned int offsetsFromUTF8[6] =
{
    0x00000000UL, 0x00003080UL, 0x000E2080UL,
    0x03C82080UL, 0xFA082080UL, 0x82082080UL
};

static const unsigned char trailingBytesForUTF8[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

int bbx_isutf8z(const char *str)
{
  int len = 0;
  int pos = 0;
  int nb;
  int i;
  int ch;

  while(str[len])
    len++;
  while(pos < len && *str)
  {
    nb = bbx_utf8_skip(str);
    if(nb < 1 || nb > 4)
      return 0;
    if(pos + nb > len)
      return 0;
    for(i=1;i<nb;i++)
      if( (str[i] & 0xC0) != 0x80 )
        return 0;
    ch = bbx_utf8_getch(str);
    if(ch < 0x80)
    {
      if(nb != 1)
        return 0;
    }
    else if(ch < 0x8000)
    {
      if(nb != 2)
        return 0;
    }
    else if(ch < 0x10000)
    {
      if(nb != 3)
        return 0;
    }
    else if(ch < 0x110000)
    {
      if(nb != 4)
        return 0;
    }
    pos += nb;
    str += nb;
  }

  return 1;
}

int bbx_utf8_skip(const char *utf8)
{
  return trailingBytesForUTF8[(unsigned char) *utf8] + 1;
}

int bbx_utf8_getch(const char *utf8)
{
    int ch;
    int nb;

    nb = trailingBytesForUTF8[(unsigned char)*utf8];
    ch = 0;
    switch (nb)
    {
            /* these fall through deliberately */
        case 3: ch += (unsigned char)*utf8++; ch <<= 6;
        case 2: ch += (unsigned char)*utf8++; ch <<= 6;
        case 1: ch += (unsigned char)*utf8++; ch <<= 6;
        case 0: ch += (unsigned char)*utf8++;
    }
    ch -= offsetsFromUTF8[nb];
    
    return ch;
}

int bbx_utf8_putch(char *out, int ch)
{
  char *dest = out;
  if (ch < 0x80)
  {
     *dest++ = (char)ch;
  }
  else if (ch < 0x800)
  {
    *dest++ = (ch>>6) | 0xC0;
    *dest++ = (ch & 0x3F) | 0x80;
  }
  else if (ch < 0x10000)
  {
     *dest++ = (ch>>12) | 0xE0;
     *dest++ = ((ch>>6) & 0x3F) | 0x80;
     *dest++ = (ch & 0x3F) | 0x80;
  }
  else if (ch < 0x110000)
  {
     *dest++ = (ch>>18) | 0xF0;
     *dest++ = ((ch>>12) & 0x3F) | 0x80;
     *dest++ = ((ch>>6) & 0x3F) | 0x80;
     *dest++ = (ch & 0x3F) | 0x80;
  }
  else
    return 0;
  return dest - out;
}

int bbx_utf8_charwidth(int ch)
{
    if (ch < 0x80)
    {
        return 1;
    }
    else if (ch < 0x800)
    {
        return 2;
    }
    else if (ch < 0x10000)
    {
        return 3;
    }
    else if (ch < 0x110000)
    {
        return 4;
    }
    else
        return 0;
}

int bbx_utf8_Nchars(const char *utf8)
{
  int answer = 0;

  while(*utf8)
  {
    utf8 += bbx_utf8_skip(utf8);
    answer++;
  }

  return answer;
}
