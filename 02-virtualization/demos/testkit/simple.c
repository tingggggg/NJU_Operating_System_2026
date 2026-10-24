#include "testkit/testkit.h"

SystemTest(test_pass, ((const char *[]){"./simple"})) {
    tk_assert(result->exit_status == 0, "should return 0");
}

SystemTest(test_fail, ((const char *[]){"./simple"})) {
    tk_assert(result->exit_status != 0, "should return non-zero");
}

int main() {
}
