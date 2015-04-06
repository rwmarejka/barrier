/*
 * M A I N - test
 */

#include <barrier.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define	NTHR	4
#define	NITER	1000

static  barrier_t   ba;

	void *
bthread( barrier_t *ba ) {
	while ( 1 )
		barrier_wait( ba );

/* NOTREACHED */
	return( 0 );
}

int
main( int argc, char *argv[] ) {
	int		c;
	int		i;
	int		niter	= NITER;
	int		nthr	= NTHR;
	int		bound	= 0;
	pthread_attr_t	attr;

	while ( ( c = getopt( argc, argv, "i:t:b" ) ) != EOF )
		switch ( c ) {
		  case 'i' :
			niter	= atoi( optarg );
			break;

		  case 't' :
			nthr	= atoi( optarg );
			break;

		  case 'b' :
			bound	= 1;
			break;

		  default  :
			fprintf( stderr, "usage: barrier -i N -t N -b\n" );
			exit( 1 );
		}

	if ( i = barrier_init( &ba, nthr + 1 ) ) {
        fprintf( stderr, "barrer_init: %d\n", i );
        exit( 1 );
    }

	pthread_attr_init( &attr );

	if ( bound )
		pthread_attr_setscope( &attr, PTHREAD_SCOPE_SYSTEM );

	for ( i=0; i < nthr; ++i ) {
		int		n;
		pthread_t	tid;

		if ( n = pthread_create( &tid, &attr, (void *(*)( void *)) bthread, &ba ) ) {
			errno	= n;
			perror( "pthread_create" );
			exit( 1 );
		}
	}

	pthread_attr_destroy( &attr );

	for ( i=0; i < niter; ++i )
		barrier_wait( &ba );

	return( 0 );
}
