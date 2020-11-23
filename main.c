#include "font.h"
#include <ft2build.h>
#include FT_FREETYPE_H

FT_Library    library;
FT_Face       face;
FT_Matrix     matrix;                 /* transformation matrix */
FT_Vector     pen;                    /* untransformed origin  */
FT_Error      error;

#define WIDTH   240
#define HEIGHT  32


/* origin is the upper left corner */
unsigned char image[HEIGHT][WIDTH];

void
draw_bitmap( FT_Bitmap*  bitmap,
             FT_Int      x,
             FT_Int      y)
{
  FT_Int  i, j, p, q;
  FT_Int  x_max = x + bitmap->width;
  FT_Int  y_max = y + bitmap->rows;


  /* for simplicity, we assume that `bitmap->pixel_mode' */
  /* is `FT_PIXEL_MODE_GRAY' (i.e., not a bitmap font)   */

  for ( i = x, p = 0; i < x_max; i++, p++ )
  {
    for ( j = y, q = 0; j < y_max; j++, q++ )
    {
      if ( i < 0      || j < 0       ||
           i >= WIDTH || j >= HEIGHT )
        continue;

      image[j][i] |= bitmap->buffer[q * bitmap->width + p];
    }
  }
}


void
show_image( void )
{
  int  i, j;


  for ( i = 0; i < HEIGHT; i++ )
  {
    for ( j = 0; j < WIDTH; j++ )
      putchar( image[i][j] == 0 ? ' '
                                : image[i][j] < 128 ? '+'
                                                    : '*' );
    putchar( '\n' );
  }
}

#define TARGET_HEIGHT 20

void drawString(const char* string)
{
    const int len = strlen(string);
    for (int i = 0; i < len; i++) 
    {
        FT_Set_Transform(face, &matrix, &pen);

        error = FT_Load_Char(face, string[i], FT_LOAD_RENDER);
        assert(!error);

        draw_bitmap(&face->glyph->bitmap, face->glyph->bitmap_left, TARGET_HEIGHT - face->glyph->bitmap_top);

        pen.x += face->glyph->advance.x;
    }
}

int main(int argc, char *argv[])
{
    matrix.xx = (FT_Fixed)1 * 0x10000L;
    matrix.xy = (FT_Fixed)0 * 0x10000L;
    matrix.yx = (FT_Fixed)0 * 0x10000L;
    matrix.yy = (FT_Fixed)1 * 0x10000L;

    int error = FT_Init_FreeType(&library);
    assert(!error);

    error = FT_New_Face(library, "/usr/share/fonts/truetype/freefont/FreeMono.ttf", 0, &face);
    assert(!error);

    error = FT_Set_Pixel_Sizes(face, 0, 28);
    assert(!error);

    drawString("Beckys butt!!");
    show_image();
    return 0;
}
