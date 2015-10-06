#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main( int argc, char *argv[] )
{

    int tabs = 0;

    if ( argc > 1 )
        tabs = atoi( argv[1] );

    for ( int i = 0; i < 100; i++ ) {
        for ( int k = 0; k < tabs; k++ )
            printf("\t");
        printf("i = %2d\n", i);
        usleep(100000);
    }

    return 0;

} /* main */
