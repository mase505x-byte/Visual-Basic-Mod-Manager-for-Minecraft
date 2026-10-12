#include <errno.h>
#include <process.h>
#include <stdio.h>

/* Run the supplied VBScript files in order with Windows Script Host. */
int main(int argc, char **argv) {
    int i;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script.vbs> [script.vbs ...]\n", argv[0]);
        return 2;
    }

    for (i = 1; i < argc; ++i) {
        const char *script_argv[] = {"cscript.exe", "//nologo", argv[i], NULL};
        intptr_t result = _spawnvp(_P_WAIT, "cscript.exe", script_argv);

        if (result == -1) {
            perror("Unable to start cscript.exe");
            return errno ? errno : 1;
        }
        if (result != 0) {
            return (int)result;
        }
    }

    return 0;
}