/* vi:set ts=4 sw=4 expandtab:
 *
 * Copyright 2016, Chris Leishman (http://github.com/cleishm)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "../../config.h"
#include "neo4j-client.h"
#include "util.h"
#include <assert.h>


int neo4j_u8clen(const char *s, size_t n)
{
    REQUIRE(s != NULL && n > 0, -1);

    const uint8_t *b = (const uint8_t *)s;
    uint8_t c = b[0];
    if (c == '\0')
    {
        return 0;
    }
    if (c < 0x80)
    {
        // 0xxxxxxx
        return 1;
    }
    if (c < 0xC2)
    {
        // 10xxxxxx (continuation byte) or 1100000x (overlong encoding)
        goto invalid;
    }
    if (c < 0xE0)
    {
        // 110xxxxx
        if (n < 2 || (b[1] & 0x80) != 0x80)
        {
            // insufficient continuation bytes
            goto invalid;
        }
        return 2;
    }
    if (c < 0xF0)
    {
        // 1110xxxx
        if (n < 3 || (b[1] & 0x80) != 0x80 || (b[2] & 0x80) != 0x80)
        {
            // insufficient continuation bytes
            goto invalid;
        }
        if (c == 0xE0 && b[1] < 0xA0)
        {
            // 11100000 100xxxxx 10xxxxxx (overlong encoding)
            goto invalid;
        }
        if (c == 0xED && b[1] >= 0xA0)
        {
            // 11101101 101xxxxx 10xxxxxx (U+D800 through U+DFFF)
            goto invalid;
        }
        return 3;
    }
    if (c < 0xF8)
    {
        // 11110xxx
        if (n < 4 || (b[1] & 0x80) != 0x80 || (b[2] & 0x80) != 0x80 ||
                (b[3] & 0x80) != 0x80)
        {
            // insufficient continuation bytes
            goto invalid;
        }
        if (c == 0xF0 && b[1] < 0x90)
        {
            // 11110000 1000xxxx 10xxxxxx 10xxxxxx (overlong encoding)
            goto invalid;
        }
        if (c > 0xF4 || (c == 0xF4 && b[1] >= 0x90))
        {
            // 111101x1 10xxxxxx 10xxxxxx 10xxxxxx
            // or 11110100 1001xxxx 10xxxxxx 10xxxxxx (codepoint > 0x10FFFF)
            goto invalid;
        }
        return 4;
    }

invalid:
    errno = EILSEQ;
    return -1;
}


int neo4j_u8codepoint(const char *s, size_t *n)
{
    int bytes = neo4j_u8clen(s, *n);
    if (bytes < 0)
    {
        return -1;
    }

    *n = (size_t)bytes;

    const uint8_t *b = (const uint8_t *)s;
    switch (bytes)
    {
    case 0:
        return 0;
    case 1:
        return (int)b[0];
    case 2:
        return ((int)(b[0] & 0x1F) << 6) | (b[1] & 0x3F);
    case 3:
        return ((int)(b[0] & 0x0F) << 12) | ((int)(b[1] & 0x3F) << 6) |
                (b[1] & 0x3F);
    default:
        assert(bytes == 4);
        return ((int)(b[0] & 0x07) << 12) | ((int)(b[1] & 0x3F) << 6) |
                (b[1] & 0x3F);
    }
}


struct interval
{
    int first;
    int last;
};


// auxiliary function for binary search in interval table
static int bisearch(int cp, const struct interval *table, int max)
{
    int min = 0;
    int mid;

    if (cp < table[0].first || cp > table[max].last)
    {
        return 0;
    }
    while (max >= min)
    {
        mid = (min + max) / 2;
        if (cp > table[mid].last)
        {
            min = mid + 1;
        }
        else if (cp < table[mid].first)
        {
            max = mid - 1;
        }
        else
        {
            return 1;
        }
    }

    return 0;
}


// implementation based on https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
int neo4j_u8cpwidth(int cp)
{
    // sorted list of non-overlapping intervals of non-spacing characters
    // generated by "uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B c"
    // (https://www.cl.cam.ac.uk/~mgk25/download/uniset.tar.gz)
    static const struct interval combining[] = {
      { 0x0300, 0x0357 }, { 0x035D, 0x036F }, { 0x0483, 0x0486 },
      { 0x0488, 0x0489 }, { 0x0591, 0x05A1 }, { 0x05A3, 0x05B9 },
      { 0x05BB, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
      { 0x05C4, 0x05C4 }, { 0x0600, 0x0603 }, { 0x0610, 0x0615 },
      { 0x064B, 0x0658 }, { 0x0670, 0x0670 }, { 0x06D6, 0x06E4 },
      { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED }, { 0x070F, 0x070F },
      { 0x0711, 0x0711 }, { 0x0730, 0x074A }, { 0x07A6, 0x07B0 },
      { 0x0901, 0x0902 }, { 0x093C, 0x093C }, { 0x0941, 0x0948 },
      { 0x094D, 0x094D }, { 0x0951, 0x0954 }, { 0x0962, 0x0963 },
      { 0x0981, 0x0981 }, { 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 },
      { 0x09CD, 0x09CD }, { 0x09E2, 0x09E3 }, { 0x0A01, 0x0A02 },
      { 0x0A3C, 0x0A3C }, { 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 },
      { 0x0A4B, 0x0A4D }, { 0x0A70, 0x0A71 }, { 0x0A81, 0x0A82 },
      { 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0AC5 }, { 0x0AC7, 0x0AC8 },
      { 0x0ACD, 0x0ACD }, { 0x0AE2, 0x0AE3 }, { 0x0B01, 0x0B01 },
      { 0x0B3C, 0x0B3C }, { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 },
      { 0x0B4D, 0x0B4D }, { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 },
      { 0x0BC0, 0x0BC0 }, { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 },
      { 0x0C46, 0x0C48 }, { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 },
      { 0x0CBC, 0x0CBC }, { 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 },
      { 0x0CCC, 0x0CCD }, { 0x0D41, 0x0D43 }, { 0x0D4D, 0x0D4D },
      { 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 }, { 0x0DD6, 0x0DD6 },
      { 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E },
      { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC },
      { 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 },
      { 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E },
      { 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 }, { 0x0F90, 0x0F97 },
      { 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 },
      { 0x1032, 0x1032 }, { 0x1036, 0x1037 }, { 0x1039, 0x1039 },
      { 0x1058, 0x1059 }, { 0x1160, 0x11FF }, { 0x1712, 0x1714 },
      { 0x1732, 0x1734 }, { 0x1752, 0x1753 }, { 0x1772, 0x1773 },
      { 0x17B4, 0x17B5 }, { 0x17B7, 0x17BD }, { 0x17C6, 0x17C6 },
      { 0x17C9, 0x17D3 }, { 0x17DD, 0x17DD }, { 0x180B, 0x180D },
      { 0x18A9, 0x18A9 }, { 0x1920, 0x1922 }, { 0x1927, 0x1928 },
      { 0x1932, 0x1932 }, { 0x1939, 0x193B }, { 0x200B, 0x200F },
      { 0x202A, 0x202E }, { 0x2060, 0x2063 }, { 0x206A, 0x206F },
      { 0x20D0, 0x20EA }, { 0x302A, 0x302F }, { 0x3099, 0x309A },
      { 0xFB1E, 0xFB1E }, { 0xFE00, 0xFE0F }, { 0xFE20, 0xFE23 },
      { 0xFEFF, 0xFEFF }, { 0xFFF9, 0xFFFB }, { 0x1D167, 0x1D169 },
      { 0x1D173, 0x1D182 }, { 0x1D185, 0x1D18B }, { 0x1D1AA, 0x1D1AD },
      { 0xE0001, 0xE0001 }, { 0xE0020, 0xE007F }, { 0xE0100, 0xE01EF }
    };

    // test for 8-bit control characters
    if (cp < 32 || (cp >= 0x7F && cp < 0xA0))
    {
        return -1;
    }

    // binary search in table of non-spacing characters
    if (bisearch(cp, combining,
                sizeof(combining) / sizeof(struct interval) - 1))
    {
        return 0;
    }

    return 1 +
        ((cp >= 0x1100 &&
          (cp <= 0x115F ||                    // Hangul Jamo init. consonants
           cp == 0x2329 || cp == 0x232A ||
           (cp >= 0x2E80 && cp <= 0xA4Cf && cp != 0x303F) || // CJK ... Yi
           (cp >= 0xAC00 && cp <= 0xD7A3) || // Hangul Syllables
           (cp >= 0xF900 && cp <= 0xFAFF) || // CJK Compatibility Ideographs
           (cp >= 0xFE10 && cp <= 0xFE19) || // Vertical forms
           (cp >= 0xFE30 && cp <= 0xFE6F) || // CJK Compatibility Forms
           (cp >= 0xFF00 && cp <= 0xFF60) || // Fullwidth Forms
           (cp >= 0xFFE0 && cp <= 0xFFE6) ||
           (cp >= 0x20000 && cp <= 0x2FFFD) ||
           (cp >= 0x30000 && cp <= 0x3FFFD)))? 1 : 0);
}


int neo4j_u8cwidth(const char *s, size_t n)
{
    int cp = neo4j_u8codepoint(s, &n);
    if (cp < 0)
    {
        return -1;
    }
    return neo4j_u8cpwidth(cp);
}


int neo4j_u8cswidth(const char *s, size_t n)
{
    int width = 0;
    do
    {
        size_t b = n;
        int cp = neo4j_u8codepoint(s, &b);
        if (cp < 0)
        {
            return -1;
        }
        int w = neo4j_u8cpwidth(cp);
        if (w < 0)
        {
            return -1;
        }
        width += w;
        assert(b <= n);
        s += b;
        n -= b;
    } while (n > 0 && *s != '\0');

    return width;
}