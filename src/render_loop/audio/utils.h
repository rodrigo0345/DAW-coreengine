//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_UTILS_H
#define DAWCOREENGINE_UTILS_H

namespace coreengine {
    enum class NoteName : int {
        C  = 0,  Cs = 1,  Db = 1,  // Enharmonic equivalents share the same index
        D  = 2,  Ds = 3,  Eb = 3,
        E  = 4,
        F  = 5,  Fs = 6,  Gb = 6,
        G  = 7,  Gs = 8,  Ab = 8,
        A  = 9,  As = 10, Bb = 10,
        B  = 11
    };

    struct NoteUtils {
        static constexpr int getMidi(const int octave, const NoteName noteIndex) {
            // noteIndex: 0=C, 1=C#, 2=D, etc.
            return (octave + 1) * 12 + static_cast<int>(noteIndex);
        }
    };
}
#endif //DAWCOREENGINE_UTILS_H