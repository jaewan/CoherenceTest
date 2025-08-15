#include "test_minimal.h"
#include <stdio.h>

void test_function(test_type_t* value) {
    *value = 42;
}

int main() {
    test_type_t value;
    test_function(&value);
    printf("Value: %d\n", value);
    return 0;
}
