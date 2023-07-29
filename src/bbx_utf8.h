//
//  bbx_utf8.h
//  babyxrc
//
//  Created by Malcolm McLean on 29/07/2023.
//

#ifndef bbx_utf8_h
#define bbx_utf8_h

int bbx_isutf8z(const char *str);
int bbx_utf8_skip(const char *utf8);
int bbx_utf8_getch(const char *utf8);
int bbx_utf8_putch(char *out, int ch);
int bbx_utf8_charwidth(int ch);
int bbx_utf8_Nchars(const char *utf8);

#endif /* bbx_utf8_h */
