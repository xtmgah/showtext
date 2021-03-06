#include "fonts.h"
#include "utf8.h"
#include "util.h"


int utf8toucs4(unsigned int *ucs4, const char *utf8, int n)
{
    int len = 0;
    int step = 0;
    int err;
    unsigned int *p1;
    const char * p2;
    for(p1 = ucs4, p2 = utf8; ; p1++, p2 += step)
    {
        /* if we meet '\0' */
        if(!p2[0]) break;
        err = TY_(DecodeUTF8BytesToChar)(p1, p2[0], p2 + 1, &step);
        if(err)
        {
            Rf_warning("UTF-8 decoding error for '%s'", utf8);
            *p1 = 0xFFFD; /* replacement char */
        }
        len++;
        if(len >= n) break;
    }
    return len;
}

/* a small example */
/*
SEXP utf8toint(SEXP str)
{
    const char *s = CHAR(STRING_ELT(str, 0));
    int maxlen = strlen(s);
    unsigned int *buf = calloc(maxlen + 1, sizeof(int));
    int len = utf8toucs4(buf, s, maxlen);
    SEXP res;
    int i;
    PROTECT(res = allocVector(INTSXP, len));
    for(i = 0; i < len; i++)
        INTEGER(res)[i] = buf[i];
    UNPROTECT(1);
    free(buf);
    return res;
}
*/



FT_Face GetFTFace(const pGEcontext gc)
{
    int gcfontface = gc->fontface;
    FontDesc *font;
    
    SEXP fontList;
    SEXP fontNames;
    SEXP extPtr;
    int i, listLen;
    
    /* Font list is sysfonts:::.pkg.env$.font.list,
       defined in sysfonts/R/font.R */    
    fontList = GetVarFromPkgEnv(".font.list", "sysfonts");
    
    /* Search the given family name */
    fontNames = GET_NAMES(fontList);
    listLen = Rf_length(fontList);
    for(i = 0; i < listLen; i++)
    {
        if(strcmp(gc->fontfamily, CHAR(STRING_ELT(fontNames, i))) == 0)
        {
            break;
        }
    }
    /* If not found, search "wqy-microhei" */
    if(i == listLen)
    {
        for(i = 0; i < listLen; i++)
        {
            if(strcmp("wqy-microhei", CHAR(STRING_ELT(fontNames, i))) == 0)
            {
                break;
            }
        }
    }
    /* If still not found, use "sans" */
    if(i == listLen) i = 0;
    if(gcfontface < 1 || gcfontface > 5) gcfontface = 1;
    
    extPtr = VECTOR_ELT(VECTOR_ELT(fontList, i), gcfontface - 1);
    font = (FontDesc *) R_ExternalPtrAddr(extPtr);
    
    return font->face;
}

/* Errors that may occur in loading font characters.
   Here we just give warnings. */
void FTError(FT_Error err)
{
    switch(err)
    {
        case 0x10:
            Rf_warning("freetype: invalid glyph index");
            break;
        case 0x11:
            Rf_warning("freetype: invalid character code");
            break;
        case 0x12:
            Rf_warning("freetype: unsupported glyph image format");
            break;
        case 0x13:
            Rf_warning("freetype: cannot render this glyph format");
            break;
        case 0x14:
            Rf_warning("freetype: invalid outline");
            break;
        case 0x15:
            Rf_warning("freetype: invalid composite glyph");
            break;
        case 0x16:
            Rf_warning("freetype: too many hints");
            break;
        case 0x17:
            Rf_warning("freetype: invalid pixel size");
            break;
        default:
            Rf_warning("freetype: error code %d", err);
            break;
    }
}

/* Get the bounding box of a string, with a possible rotation */
void GetStringBBox(FT_Face face, const unsigned int *str, int nchar, double rot,
                   int *xmin, int *xmax, int *ymin, int *ymax)
{
    int char_xmin, char_xmax, char_ymin, char_ymax;
    FT_GlyphSlot slot = face->glyph;
    FT_Matrix trans;
    FT_Vector pen;
    FT_Error err;
    int i;
    
    *xmin = *xmax = *ymin = *ymax = 0;
    
    /* Set rotation transformation */
    trans.xx = trans.yy = (FT_Fixed)( cos(rot) * 0x10000L);
    trans.xy = (FT_Fixed)(-sin(rot) * 0x10000L);
    trans.yx = -trans.xy;
    pen.x = pen.y = 0;

    for(i = 0; i < nchar; i++)
    {
        FT_Set_Transform(face, &trans, &pen);
        err = FT_Load_Char(face, str[i], FT_LOAD_RENDER);
        if(err)  FTError(err);
        char_xmin = slot->bitmap_left;
        char_xmax = char_xmin + slot->bitmap.width;
        char_ymax = slot->bitmap_top;
        char_ymin = char_ymax - slot->bitmap.rows;
        if(i == 0)
        {
            *xmin = char_xmin;
            *xmax = char_xmax;
            *ymin = char_ymin;
            *ymax = char_ymax;
        } else {
            *xmin = char_xmin < *xmin ? char_xmin : *xmin;
            *xmax = char_xmax > *xmax ? char_xmax : *xmax;
            *ymin = char_ymin < *ymin ? char_ymin : *ymin;
            *ymax = char_ymax > *ymax ? char_ymax : *ymax;
        }
        /* Move the pen for the next character */
        pen.x += slot->advance.x;
        pen.y += slot->advance.y;
    }
    
    /* Identity transformation */
    trans.xx = trans.yy = 0x10000L;
    trans.xy = trans.yx = 0;
    pen.x = pen.y = 0;
    /* Restore to identity */
    FT_Set_Transform(face, &trans, &pen);
}
