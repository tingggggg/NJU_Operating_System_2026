#include <stdio.h>
#include <stdint.h>

// Global variable - stored in data segment
int global_var = 0x12345678;

// Constant variable - stored in text segment (read-only data)
const int const_var = 0xabcdef12;

int main() {
    // Stack variables
    int stack_var1 = 0xdeadbeef;
    int stack_var2 = 0xcafebabe;

    // Print addresses
    printf("=== Memory Address Demonstration ===\n\n");

    // 1. Code segment (text segment) - where main function resides
    printf("1. Code Segment (Text Segment):\n");
    printf("   main function address  : %p\n", (void*)main);

    // 2. Data segment - global variables
    printf("\n2. Data Segment:\n");
    printf("   global_var address     : %p\n", (void*)&global_var);
    printf("   global_var value       : 0x%x\n", global_var);

    // 3. Read-only data segment (part of text segment on some systems)
    printf("\n3. Read-only Data Segment:\n");
    printf("   const_var address      : %p\n", (void*)&const_var);
    printf("   const_var value        : 0x%x\n", const_var);

    // 4. Stack segment - local variables
    printf("\n4. Stack Segment:\n");
    printf("   stack_var1 address     : %p\n", (void*)&stack_var1);
    printf("   stack_var1 value       : 0x%x\n", stack_var1);
    printf("   stack_var2 address     : %p\n", (void*)&stack_var2);
    printf("   stack_var2 value       : 0x%x\n", stack_var2);
    printf("   Note: Stack growth direction depends on architecture\n");

    // Verify that main's address contains executable code
    printf("\n=== Verifying Main Function Binary Code ===\n");

    // Cast main function pointer to uint8_t* to read byte by byte
    uint8_t *main_code = (uint8_t*)main;

    printf("First 20 bytes of main() function (hex):\n");
    for (int i = 0; i < 20; i++) {
        printf("0x%02x ", main_code[i]);
        if ((i + 1) % 8 == 0)
            printf("\n");
    }
    if (20 % 8 != 0)
        printf("\n");

    return 0;
}
