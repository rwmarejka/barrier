/*
 * POSIX threads barrier.
 *
 */

#pragma ident "%Z% %M% %I% %E%, Richard.Marejka@Sun.COM"

/* Feature Test Macros		*/

#define BARRIER_VERSION     (3)

/* Include Files		*/

#include <barrier.h>

/* Constants and Macros		*/

/* Data Definitions		*/

/* External References		*/

/* External Declarations	*/

static	const unsigned	BARRIER_LIMIT_DEFAULT	= 1;
static	const int	BARRIER_PSHARED_DEFAULT	= PTHREAD_PROCESS_PRIVATE;

/**
 * barrier_init - initialize a barrier.
 *
 *	@param	bp	pointer to unitialized barrier.
 *	@param	ap	pointer to initialized barrier attribute or NULL.
 *
 *	@return	Returns zero (0) for success or an errno-value.
 */

int
barrier_init( barrier_t *bp, unsigned count ) {
	int	status	= 0;

	pthread_mutex_init( &bp->lk, NULL );
	pthread_cond_init( &bp->cv, NULL );

	bp->limit	= count;
	bp->runners	= count;
	bp->generation	= 0;

	return status;
}

/**
 * barrier_destroy - destroy a barrier.
 *
 *	@param	bp	pointer to initialized barrier.
 *
 *	@return	Returns zero (0) on success or an errno-value.
 */

int
barrier_destroy( barrier_t *bp ) {
	int	status	= 0;

	pthread_mutex_lock( &bp->lk );

	bp->limit	= 0;
	bp->runners	= 0;
	bp->generation	= 0;

	pthread_cond_destroy( &bp->cv );
	pthread_mutex_unlock( &bp->lk );
	pthread_mutex_destroy( &bp->lk );

	return status;
}

/**
 * barrier_wait - wait at a barrier.
 *
 *	@param	bp	pointer to an initialized barrier.
 *
 *	@return	Returns zero (0) on success or an errno-value.
 */

int
barrier_wait( barrier_t *bp ) {
	int	status	= 0;

	pthread_mutex_lock( &bp->lk );

	if ( bp->runners == 1 ) {	/* last thread to reach barrier			*/
		bp->generation++;		/* crank generation number		*/
		bp->runners	= bp->limit;	/* reset number of running threads	*/

		pthread_cond_broadcast( &bp->cv );
	} else {
		int		cs_prev;
		int		cs_ignore;
		unsigned	genx	= bp->generation;

		bp->runners--;			/* one less running thread		*/

		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs_prev );

					/* wait until the generation number changes.	*/
		while ( bp->generation == genx )
			pthread_cond_wait( &bp->cv, &bp->lk );

		pthread_setcancelstate( cs_prev, &cs_ignore );
	}

	pthread_mutex_unlock( &bp->lk );

	return status;
}