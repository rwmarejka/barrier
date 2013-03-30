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
	int		pshared	= PTHREAD_PROCESS_PRIVATE;
	int		niter	= NITER;
	int		nthr	= NTHR;
	int		bound	= 0;
	pthread_attr_t	attr;

	while ( ( c = getopt( argc, argv, "i:t:bps" ) ) != EOF )
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

		  case 'p' :
			pshared	= PTHREAD_PROCESS_PRIVATE;
			break;

		  case 's' :
			pshared	= PTHREAD_PROCESS_SHARED;
			break;

		  default  :
			fprintf( stderr, "usage: barrier -i N -t N -b -p -s\n" );
			exit( 1 );
		}

	barrier_init( &ba, nthr + 1 );
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
