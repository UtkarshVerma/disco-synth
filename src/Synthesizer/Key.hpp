#pragma once

#include <cstdint>

class Key {
   private:
    unsigned int key;

   public:
    enum {
        A3,
        BB3,
        B3,
        C3,
        CH3,
        D3,
        DH3,
        E3,
        F3,
        FH3,
        G3,
        AB3,
        A4,
        BB4,
        B4,
        C4,
        CH4,
        D4,
        DH4,
        E4,

        COUNT,
    };

    /// @brief Generate Key from keyboard input
    /// @param c the keyboard input
    /// @return keyboard key
    Key(char ch);

    /// @brief Get the frequency associated with this key in millihertz.
    /// @return frequency in millihertz
    uint32_t freq_millihz(void);

    bool operator==(const Key& other);
};
