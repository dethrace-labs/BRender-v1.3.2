#include "drv.h"

// todo assert or warn based on a flag
void GL_AssertOrWarnIfError(char *file, int line) {
    int e;
    e = glGetError();
    if (e != 0) {
        BrWarning("GL error: %s:%d %d\n", file, line, e);
    }
}
