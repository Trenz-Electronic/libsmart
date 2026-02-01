#include <cstdio>
#include "wav_verify.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: wav-verify FILE...\n");
        return 1;
    }

    bool any_errors = false;
    for (int i = 1; i < argc; ++i) {
        auto r = wav_verify_file(argv[i]);
        printf("%s: %s\n", argv[i], r.valid ? "OK" : "FAIL");
        if (!r.valid) {
            printf("%s", r.summary().c_str());
            any_errors = true;
        }
    }
    return any_errors ? 1 : 0;
}
