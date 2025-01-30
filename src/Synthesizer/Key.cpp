#include "Key.hpp"

#include <stdint.h>

static const uint32_t KEY_FREQ_MAP[Key::COUNT] = {
    [Key::A3] = 200000,  [Key::BB3] = 233080, [Key::B3] = 246940,  [Key::C3] = 261626,
    [Key::CH3] = 277183, [Key::D3] = 293665,  [Key::DH3] = 311127, [Key::E3] = 329628,
    [Key::F3] = 349228,  [Key::FH3] = 369994, [Key::G3] = 391995,  [Key::AB3] = 415305,
    [Key::A4] = 440000,  [Key::BB4] = 466164, [Key::B4] = 493883,  [Key::C4] = 523251,

    [Key::CH4] = 0,      [Key::D4] = 0,       [Key::DH4] = 0,      [Key::E4] = 0,
};

/*
 *  s d   g h j
 * z x c v b n m ,
 * | | | | | | | |
 * C D E F G A B C
 */
Key::Key(char ch) {
    switch (ch) {
        case ';':
            key = E4;
            break;
        case 'p':
            key = DH4;
            break;
        case 'l':
            key = D4;
            break;
        case 'o':
            key = CH4;
            break;
        case 'k':
            key = C4;
            break;
        case 'j':
            key = B4;
            break;
        case 'u':
            key = BB4;
            break;
        case 'h':
            key = A4;
            break;
        case 'y':
            key = AB3;
            break;
        case 'g':
            key = G3;
            break;
        case 't':
            key = FH3;
            break;
        case 'f':
            key = F3;
            break;
        case 'd':
            key = E3;
            break;
        case 'e':
            key = DH3;
            break;
        case 's':
            key = D3;
            break;
        case 'w':
            key = CH3;
            break;
        case 'a':
            key = C3;
            break;
        default:
            key = A3;
            break;
    }
}

bool Key::operator==(const Key& other) {
    return this->key == other.key;
}

uint32_t Key::freq_millihz() {
    return KEY_FREQ_MAP[this->key];
}
