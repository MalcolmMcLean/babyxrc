#include <stdlib.h>
#include <string.h>
#include <math.h>

#define clamp(x,low,high) (x) < (low) ? (low) : (x) > (high) ? (high) : (x)

/*
  resize an image using the averaging method.
  Note that dwidth and dheight must be smaller than or equal to swidth, sheight.

 */
void sprshrink(unsigned char *dest, int dwidth, int dheight, unsigned char *src, int swidth, int sheight)
{
  int x, y;
  int i, ii;
  float red, green, blue, alpha;
  float xfrag, yfrag, xfrag2, yfrag2;
  float xt, yt, dx, dy;
  int xi, yi;


  dx = ((float)swidth)/dwidth;
  dy = ((float)sheight)/dheight;

  for(yt= 0, y=0;y<dheight;y++, yt += dy)
  {
    yfrag = (float) ceil(yt) - yt;
    if(yfrag == 0)
      yfrag = 1;
    yfrag2 = yt+dy - (float) floor(yt + dy);
    if(yfrag2 == 0 && dy != 1.0f)
      yfrag2 = 1;
    
    for(xt = 0, x=0;x<dwidth;x++, xt+= dx)
    {
      xi = (int) xt;
      yi = (int) yt;
      xfrag = (float) ceil(xt) - xt;
      if(xfrag == 0)
        xfrag = 1;
      xfrag2 = xt+dx - (float) floor(xt+dx);
      if(xfrag2 == 0 && dx != 1.0f)
        xfrag2 = 1;
      red = xfrag * yfrag * src[(yi*swidth+xi)*4];
      green =  xfrag * yfrag * src[(yi*swidth+xi)*4+1];
      blue =   xfrag * yfrag * src[(yi*swidth+xi)*4+2];
      alpha =  xfrag * yfrag * src[(yi*swidth+xi)*4+3];
      
      for(i=0; xi + i + 1 < xt+dx-1; i++)
      {
        red += yfrag * src[(yi*swidth+xi+i+1)*4];
        green += yfrag * src[(yi*swidth+xi+i+1)*4+1];
        blue += yfrag * src[(yi*swidth+xi+i+1)*4+2];
        alpha += yfrag * src[(yi*swidth+xi+i+1)*4+3];
      } 
      
      red += xfrag2 * yfrag * src[(yi*swidth+xi+i+1)*4];
      green += xfrag2 * yfrag * src[(yi*swidth+xi+i+1)*4+1];
      blue += xfrag2 * yfrag * src[(yi*swidth+xi+i+1)*4+2];
      alpha += xfrag2 * yfrag * src[(yi*swidth+xi+i+1)*4+3];
      for(i=0; yi+i+1 < yt +dy-1 && yi + i+1 < sheight;i++)
      {
        red += xfrag * src[((yi+i+1)*swidth+xi)*4];
        green += xfrag * src[((yi+i+1)*swidth+xi)*4+1];
        blue += xfrag * src[((yi+i+1)*swidth+xi)*4+2];
        alpha += xfrag * src[((yi+i+1)*swidth+xi)*4+3];
        
		for (ii = 0; xi + ii + 1 < xt + dx - 1 && xi + ii + 1 < swidth; ii++)
	{
          red += src[((yi+i+1)*swidth+xi+ii+1)*4];
          green += src[((yi+i+1)*swidth+xi+ii+1)*4+1];
          blue += src[((yi+i+1)*swidth+xi+ii+1)*4+2];
          alpha += src[((yi+i+1)*swidth+xi+ii+1)*4+3];
	}
        
        red += xfrag2 * src[((yi+i+1)*swidth+xi+ii+1)*4]; 
        green += xfrag2 * src[((yi+i+1)*swidth+xi+ii+1)*4+1]; 
        blue += xfrag2 * src[((yi+i+1)*swidth+xi+ii+1)*4+2]; 
        alpha += xfrag2 * src[((yi+i+1)*swidth+xi+ii+1)*4+3]; 
      }

	  if (yi + i + 1 < sheight)
	  {
		  red += xfrag * yfrag2 * src[((yi + i + 1)*swidth + xi) * 4];
		  green += xfrag * yfrag2 * src[((yi + i + 1)*swidth + xi) * 4 + 1];
		  blue += xfrag * yfrag2 * src[((yi + i + 1)*swidth + xi) * 4 + 2];
		  alpha += xfrag * yfrag2 * src[((yi + i + 1)*swidth + xi) * 4 + 3];

		  for (ii = 0; xi + ii + 1 < xt + dx - 1 && xi + ii + 1 < swidth; ii++)
		  {
			  red += yfrag2 * src[((yi + i + 1)*swidth + xi + ii + 1) * 4];
			  green += yfrag2 * src[((yi + i + 1)*swidth + xi + ii + 1) * 4 + 1];
			  blue += yfrag2 * src[((yi + i + 1)*swidth + xi + ii + 1) * 4 + 2];
			  alpha += yfrag2 * src[((yi + i + 1)*swidth + xi + ii + 1) * 4 + 3];
		  }
	  }
 
	  if (yi + i + 1 < sheight && x + xi + 1 < swidth)
	  {
		  red += xfrag2 * yfrag2 * src[((yi + i + 1)*swidth + xi + ii + 1) * 4];
		  green += xfrag2 * yfrag2 * src[((yi + i + 1)*swidth + xi + ii + 1) * 4 + 1];
		  blue += xfrag2 * yfrag2 * src[((yi + i + 1)*swidth + xi + ii + 1) * 4 + 2];
		  alpha += xfrag2 * yfrag2 * src[((yi + i + 1)*swidth + xi + ii + 1) * 4 + 3];
	  }
     red /= dx * dy;
     green /= dx * dy;
     blue /= dx * dy;
     alpha /= dx * dy;
     red = clamp(red, 0, 255);
     green = clamp(green, 0, 255);
     blue = clamp(blue, 0, 255);
     alpha = clamp(alpha, 0, 255);
   
     dest[(y*dwidth+x)*4] = (unsigned char) red;
     dest[(y*dwidth+x)*4+1] = (unsigned char) green;
     dest[(y*dwidth+x)*4+2] = (unsigned char) blue;
     dest[(y*dwidth+x)*4+3] = (unsigned char) alpha;
    }
  }


}

/*
  resize an image using bilinear interpolation
 */
void bilerp(unsigned char *dest, int dwidth, int dheight, unsigned char *src, int swidth, int sheight)
{
  float a, b;
  float red, green, blue, alpha;
  float dx, dy;
  float rx, ry;
  int x, y;
  int index0, index1, index2, index3;

  dx = ((float) swidth)/dwidth;
  dy = ((float) sheight)/dheight;
  for(y=0, ry = 0;y<dheight-1;y++, ry += dy)
  {
    b = ry - (int) ry;
    for(x=0, rx = 0;x<dwidth-1;x++, rx += dx)
    {
      a = rx - (int) rx;
      index0 = (int)ry * swidth + (int) rx;
      index1 = index0 + 1;
      index2 = index0 + swidth;     
      index3 = index0 + swidth + 1;

      red = src[index0*4] * (1.0f-a)*(1.0f-b);
      green = src[index0*4+1] * (1.0f-a)*(1.0f-b);
      blue = src[index0*4+2] * (1.0f-a)*(1.0f-b);
      alpha = src[index0*4+3] * (1.0f-a)*(1.0f-b);
      red += src[index1*4] * (a)*(1.0f-b);
      green += src[index1*4+1] * (a)*(1.0f-b);
      blue += src[index1*4+2] * (a)*(1.0f-b);
      alpha += src[index1*4+3] * (a)*(1.0f-b);
      red += src[index2*4] * (1.0f-a)*(b);
      green += src[index2*4+1] * (1.0f-a)*(b);
      blue += src[index2*4+2] * (1.0f-a)*(b);
      alpha += src[index2*4+3] * (1.0f-a)*(b);
      red += src[index3*4] * (a)*(b);
      green += src[index3*4+1] * (a)*(b);
      blue += src[index3*4+2] * (a)*(b);
      alpha += src[index3*4+3] * (a)*(b);
    
      red = red < 0 ? 0 : red > 255 ? 255 : red;
      green = green < 0 ? 0 : green > 255 ? 255 : green;
      blue = blue < 0 ? 0 : blue > 255 ? 255 : blue;
      alpha = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;

      dest[(y*dwidth+x)*4] = (unsigned char) red;
      dest[(y*dwidth+x)*4+1] = (unsigned char) green;
      dest[(y*dwidth+x)*4+2] = (unsigned char) blue;
      dest[(y*dwidth+x)*4+3] = (unsigned char) alpha;
    }
    index0 = (int)ry * swidth + (int) rx;
    index1 = index0;
    index2 = index0 + swidth;     
    index3 = index0 + swidth;   

    red = src[index0*4] * (1.0f-a)*(1.0f-b);
    green = src[index0*4+1] * (1.0f-a)*(1.0f-b);
    blue = src[index0*4+2] * (1.0f-a)*(1.0f-b);
    alpha = src[index0*4+3] * (1.0f-a)*(1.0f-b);
    red += src[index1*4] * (a)*(1.0f-b);
    green += src[index1*4+1] * (a)*(1.0f-b);
    blue += src[index1*4+2] * (a)*(1.0f-b);
    alpha += src[index1*4+3] * (a)*(1.0f-b);
    red += src[index2*4] * (1.0f-a)*(b);
    green += src[index2*4+1] * (1.0f-a)*(b);
    blue += src[index2*4+2] * (1.0f-a)*(b);
    alpha += src[index2*4+3] * (1.0f-a)*(b);
    red += src[index3*4] * (a)*(b);
    green += src[index3*4+1] * (a)*(b);
    blue += src[index3*4+2] * (a)*(b);
    alpha += src[index3*4+3] * (a)*(b);
        
    red = red < 0 ? 0 : red > 255 ? 255 : red;
    green = green < 0 ? 0 : green > 255 ? 255 : green;
    blue = blue < 0 ? 0 : blue > 255 ? 255 : blue;

    alpha = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;
    dest[(y*dwidth+x)*4] = (unsigned char) red;
    dest[(y*dwidth+x)*4+1] = (unsigned char) green;
    dest[(y*dwidth+x)*4+2] = (unsigned char) blue;
    dest[(y*dwidth+x)*4+3] = (unsigned char) alpha;
  }
  index0 = (int)ry * swidth + (int) rx;
  index1 = index0;
  index2 = index0 + swidth;     
  index3 = index0 + swidth;   

  for(x=0, rx = 0;x<dwidth-1;x++, rx += dx)
  {
    a = rx - (int) rx;
    index0 = (int)ry * swidth + (int) rx;
    index1 = index0 + 1;
    index2 = index0;     
    index3 = index0;

    red = src[index0*4] * (1.0f-a)*(1.0f-b);
    green = src[index0*4+1] * (1.0f-a)*(1.0f-b);
    blue = src[index0*4+2] * (1.0f-a)*(1.0f-b);
    alpha = src[index0*4+3] * (1.0f-a)*(1.0f-b);
    red += src[index1*4] * (a)*(1.0f-b);
    green += src[index1*4+1] * (a)*(1.0f-b);
    blue += src[index1*4+2] * (a)*(1.0f-b);
    alpha += src[index1*4+3] * (a)*(1.0f-b);
    red += src[index2*4] * (1.0f-a)*(b);
    green += src[index2*4+1] * (1.0f-a)*(b);
    blue += src[index2*4+2] * (1.0f-a)*(b);
    alpha += src[index2*4+3] * (1.0f-a)*(b);
    red += src[index3*4] * (a)*(b);
    green += src[index3*4+1] * (a)*(b);
    blue += src[index3*4+2] * (a)*(b);
    alpha += src[index3*4+3] * (a)*(b);

    red = red < 0 ? 0 : red > 255 ? 255 : red;
    green = green < 0 ? 0 : green > 255 ? 255 : green;
    blue = blue < 0 ? 0 : blue > 255 ? 255 : blue;
    alpha = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;
      
    dest[(y*dwidth+x)*4] = (unsigned char) red;
    dest[(y*dwidth+x)*4+1] = (unsigned char) green;
    dest[(y*dwidth+x)*4+2] = (unsigned char) blue;
    dest[(y*dwidth+x)*4+3] = (unsigned char) alpha;
  }
  
  dest[(y*dwidth+x)*4] = src[((sheight-1)*swidth+swidth-1)*4];
  dest[(y*dwidth+x)*4+1] = src[((sheight-1)*swidth+swidth-1)*4+1];
  dest[(y*dwidth+x)*4+2] = src[((sheight-1)*swidth+swidth-1)*4+2];
  dest[(y*dwidth+x)*4+3] = src[((sheight-1)*swidth+swidth-1)*4+3];
}  


/*
  resize a 23 bit rgba image
  Notes:
    uses bilinear interpolation to expand, averaging to shrink
 */
void resizeimage(unsigned char *dest, int dwidth, int dheight, unsigned char *src, int swidth, int sheight)
{
  if(dwidth == swidth && sheight == dheight)
    memcpy(dest, src, dwidth*dheight*4);
  else if(dwidth > swidth || dheight > sheight)
    bilerp(dest, dwidth, dheight, src, swidth, sheight);
  else
    sprshrink(dest, dwidth, dheight, src, swidth, sheight);
} 

#include <stdio.h>
#include "lodepng.h"
int resizemain(void)
{
  unsigned char *rgba;
  unsigned int uwidth, uheight;
  int width, height;
  unsigned char *out;

  lodepng_decode32_file(&rgba, &uwidth, &uheight, "ponies.png");
  width = (int) uwidth;
  height = (int) uheight;
  out = malloc(width * height * 4 * 4);
  printf("here width %d height %d\n", width, height);
  sprshrink(out, width*2, height*2, rgba, width, height);
  lodepng_encode32_file("shrunk.png", out, width*2, height*2);

  return 0;

}
