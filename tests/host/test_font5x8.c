#include <stdint.h>
#include "minitest.h"
#include "font5x8.h"

TEST(utf8_ascii)     { const char *s = "A"; ASSERT_EQ_INT(utf8_next(&s), 'A'); ASSERT_EQ_INT(utf8_next(&s), 0); }
TEST(utf8_cyrillic)  { const char *s = "Ґї"; ASSERT_EQ_INT(utf8_next(&s), 0x490); ASSERT_EQ_INT(utf8_next(&s), 0x457); }
TEST(utf8_bad_byte)  { const char *s = "\xff" "A"; ASSERT_EQ_INT(utf8_next(&s), 0xFFFD); ASSERT_EQ_INT(utf8_next(&s), 'A'); }
TEST(utf8_emoji_4b)  { const char *s = "\xF0\x9F\x98\x80"; ASSERT_EQ_INT(utf8_next(&s), 0x1F600); ASSERT_EQ_INT(utf8_next(&s), 0); }
TEST(utf8_truncated_seq) { const char *s = "\xD0"; ASSERT_EQ_INT(utf8_next(&s), 0xFFFD); ASSERT_EQ_INT(utf8_next(&s), 0); }
TEST(glyph_space_blank) { const uint8_t *g = font5x8_glyph(' '); for (int i = 0; i < 5; i++) ASSERT_EQ_INT(g[i], 0); }
TEST(glyph_known_differs_from_unknown) {
    ASSERT_TRUE(font5x8_glyph(0x490) != font5x8_glyph('?'));
    ASSERT_TRUE(font5x8_glyph(0x1F600) == font5x8_glyph('?'));
}
TEST(all_ukrainian_present) {
    const char *s = "АБВГҐДЕЄЖЗИІЇЙКЛМНОПРСТУФХЦЧШЩЬЮЯабвгґдеєжзиіїйклмнопрстуфхцчшщьюя’—°";
    uint32_t c;
    while ((c = utf8_next(&s))) ASSERT_TRUE(font5x8_glyph(c) != font5x8_glyph('?'));
}

TEST(capital_yi_differs_from_i) {
    /* X11 5x8 draws Ї without dots; the generator overrides it */
    const uint8_t *yi = font5x8_glyph(0x407), *i = font5x8_glyph(0x406);
    int same = 1;
    for (int k = 0; k < 5; k++) same &= yi[k] == i[k];
    ASSERT_TRUE(!same);
}

int main(void) {
    RUN(utf8_ascii); RUN(utf8_cyrillic); RUN(utf8_bad_byte); RUN(utf8_emoji_4b); RUN(utf8_truncated_seq);
    RUN(glyph_space_blank); RUN(glyph_known_differs_from_unknown); RUN(all_ukrainian_present); RUN(capital_yi_differs_from_i);
    return MT_RESULT();
}
