#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    //for(int i = 0; i < argc; i++) printf("<%s>\n", argv[i]);
    if(1 < argc) printf("<size: %ld, text: %s>\n", strlen(argv[1]), argv[1]);
    return 0;
}