#include <stdio.h>
#include <string.h>

#include "utils/osmem.h"
#include "utils/printf.h"

int main(void)
{
    char *s = os_calloc(100, 1);
    scanf("%s", s);
    printf("%s\n", s);
    os_free(s);
    return 0;
}