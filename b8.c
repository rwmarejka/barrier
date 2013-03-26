
/*
 * POSIX barrier.
 *
 */

#pragma ident "%Z% %M% %I% %E%, Richard.Marejka@Sun.COM"

/* Feature Test Macros		*/

#define BARRIER_VERSION     (8)

/* Include Files		*/

#include <barrier.h>

/* Constants and Macros		*/

/*
 * Since we're using assert "live" in code, make certain that the code does
 * not get pre-processed away. Note that this has to occur after all include
 * file references. See: assert(3C).
 */

/* Data Definitions		*/

/* External References		*/

/* External Declarations	*/

static	const unsigned		BARRIER_LIMIT_DEFAULT	= 1;
static	const int		BARRIER_PSHARED_DEFAULT	= PTHREAD_PROCESS_PRIVATE;

/**
 * barrier_init - initialize a barrier.
 *
 *	@param	bp	reference to barrier object.
 *	@param	ap	reference to barrier attribute object or NULL.
 *
 *	@return		Returns zero (0) for success, otherwise an errno-value is returned.
 */

int
barrier_init( barrier_t *bp, unsigned count ) {
	int	status	= 0;

	if ( bp->waiters = calloc( count, sizeof( SEM_TYPE ) ) ) {
                unsigned    i;

		pthread_mutex_init( &bp->lk, NULL );

                for ( i=0; i < count; ++i )
                    SEM_INIT( &(bp->waiters[i]) )

		bp->limit	= count;
		bp->runners	= count;
	} else
		status	= ENOMEM;

	return status;
}

/**
 * barrier_destroy - destroy a barrier.
 *
 *	@param	bp	reference to a barrier object.
 *
 *	@return		Returns zero (0) on success or an errno-value.
 *
 *	@note		Usage of the barrier after pthread_barrier_destroy_np is undefined.
 */

int
barrier_destroy( barrier_t *bp ) {
	int	status	= 0;

	pthread_mutex_lock( &bp->lk );

	{
		unsigned	i;
		unsigned	limit	= bp->limit;

                for ( i=0; i < limit; ++i )
                    SEM_DESTROY( &(bp->waiters[i]));

		free( bp->waiters );
	}

	bp->limit	= 0;
	bp->runners	= 0;
	bp->waiters	= NULL;

	pthread_mutex_unlock( &bp->lk );
	pthread_mutex_destroy( &bp->lk );

	return status;
}

/**
 * barrier_wait - wait at a barrier.
 *
 *	@param	bp	reference to a barrier object.
 *
 *	@return		Returns zero (0) on success or an errno-value.
 *
 *	@note		This interface is not a cancellation point.
 */

int
barrier_wait( barrier_t *bp ) {
	int		status	= 0;
	SEM_TYPE	*wp;

	pthread_mutex_lock( &bp->lk );

	if ( bp->runners == 1 ) {	/* last thread to reach barrier	*/
		int		i	= 1;
		int		limit	= bp->limit;
		SEM_TYPE	**wlp	= bp->waiters;

		/*
		 * Run the list of waiters, waking each using a SEM_POST. Then
		 * reset the current number of runners to the limit value.
		 */

		for ( ; i < limit; ++i )
			SEM_POST( *(++wlp) );

		 bp->runners	= bp->limit;

		pthread_mutex_unlock( &bp->lk );
	} else {
		int	cs_old;
		int	cs_ignore;

		/*
		 * Not the last thread at the barrier... so get our wait
		 * object, enqueue it on the wait list and wait for everyone
		 * else to arrive.
		 *
		 * XXX - The value of the semaphore is forced to zero.
		 */

                wp  = &( bp->waiters[ --bp->runners ]);

		pthread_mutex_unlock( &bp->lk );
		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs_old );

		while ( SEM_WAIT( wp ) == EINTR )
			/* LINTED - empty loop	*/
			;

		pthread_setcancelstate( cs_old, &cs_ignore );
	}

	return status;
}
