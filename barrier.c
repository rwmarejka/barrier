/*
%  cc -DUNITTEST=0 -I. -mt -Xa -v -c -O barrier.c
%t cc -DUNITTEST=1 -I. -mt -Xa -v -g -o barrier barrier.c -lpthread -lthread
%l cc -I. -mt -Xa -v -xO4 -G -Kpic -z defs -o barrier.so barrier.c -lthread -lc
 *
 * BARRIER(3T) - implement a barrier mechanism.
 *
 *	int	barrier_init( barrier_t *bp, int count, int type, void *arg );
 *	int	barrier_wait( barrier_t *bp );
 *	int	barrier_destroy( barrier_t *bp );
 *
 * written : Richard.Marejka@Sun.COM 1995.07.29
 * modified: Richard.Marejka@Sun.COM 2001.03.02
 *		- error detection
 *		- pshared member added for semaphore usage
 *		- compile-time choice of Unix Intl or POSIX semaphore.
 */

#pragma ident "@(#) barrier.c 1.16 02/06/10 Richard.Marejka@Sun.COM"

/* Feature Test Macros	*/

#define BARRIER_VERSION     (7)

/* Include Files	*/

#include <barrier.h>
#include <assert.h>

/* Constants & Macros	*/

/* Data Declarations	*/

/* External Declarations	*/

#if (BARRIER_VERSION == 3) || (BARRIER_VERSION == 6) || (BARRIER_VERSION == 8)
static	const unsigned  BARRIER_LIMIT_DEFAULT	= 1;
static	const int       BARRIER_PSHARED_DEFAULT	= PTHREAD_PROCESS_PRIVATE;
#endif

/* P R I V A T E   I N T E R F A C E S			*/

#if (BARRIER_VERSION == 7)
#if !defined(__sun)

/*
 * If we are not on a Sun system (i.e. Solaris), then map the Solaris
 * semaphore interfaces to the POSIX semaphore interfaces. Make the
 * interfaces private (read: static) so as not to pollute the ABI of
 * wherever we are running (i.e. not Solaris).
 */

static int
sema_init( sema_t *sp, unsigned value, int type, void *arg ) {
	int	pshared;

	switch ( type ) {
	  case USYNC_PROCESS	:
		pshared	= PTHREAD_SCOPE_SYSTEM;
		break;

	  case USYNC_THREAD	:
		pshared	= PTHREAD_SCOPE_PROCESS;
		break;

	  default		:
		return( EINVAL );
		/*NOTREACHED*/
	}

	return( sem_init( sp, pshared, value ) );
}

static int
sema_wait( sema_t *sp ) {
	return( sem_wait( sp ) );
}

static int
sema_post( sema_t *sp ) {
	return( sem_post( sp ) );
}

static int
sema_destroy( sema_t *sp ) {
	return( sem_destroy( sp ) );
}
#endif	/* !defined(__sun)	*/
#endif  /* BARRIER_VERSION == 7     */

/*
 * barrier_init - initialize a barrier variable (count,pshared).
 *
 * Return zero for success or an errno(2) value on failure.
 *
 */

	int
barrier_init( barrier_t *bp, unsigned count ) {
	int	status  = 0;

    if ( count < 1 )
        return EINVAL;

#if (BARRIER_VERSION == 1)
    
    bp->limit  = count;
    bp->sbp     = &bp->sb[0];

    for ( i=0; i < 2; ++i ) {
        struct _sb  *sbp    = &( bp->sb[i] );

        sbp->runners    = count;

        if ( status = pthread_mutex_init( &sbp->lk, NULL ) )
            return status;

        if ( status = pthread_cond_init( &sbp->cv, NULL ) )
            return status;
    }
    
#elif (BARRIER_VERSION == 3)
    
    if ( status = pthread_mutex_init( &bp->lk, NULL ) )
        return status;

	if ( status = pthread_cond_init( &bp->cv, NULL ) )
        return status;

	bp->limit	= count;
	bp->runners	= count;
	bp->generation	= 0;
    
#elif (BARRIER_VERSION == 6)
    
    if ( bp->waiters = (SEM_TYPE **) calloc( count - 1, sizeof( SEM_TYPE * ) ) ) {
		if ( status = pthread_mutex_init( &bp->lk, NULL ) )
            return status;

		bp->limit	= count;
		bp->runners	= count;
	} else
		status	= ENOMEM;
    
#elif (BARRIER_VERSION == 7)
    
	bp->limit	= count;
	bp->runners	= count;
	bp->head	= NULL;

	status	= pthread_mutex_init( &(bp->lk), NULL );
    
#elif (BARRIER_VERSION == 8)
    
    if ( bp->waiters = (SEM_TYPE *) calloc( count, sizeof( SEM_TYPE ) ) ) {
        unsigned    i;

		pthread_mutex_init( &bp->lk, NULL );

        for ( i=0; i < count; ++i )
            SEM_INIT( &(bp->waiters[i]) )

        bp->limit	= count;
		bp->runners	= count;
	} else
		status	= ENOMEM;
    
#endif

	return( status );
}

/*
 * barrier_wait - wait at a barrier for everyone to arrive.
 *
 */

	int
barrier_wait( barrier_t *bp ) {
	int	status	= 0;

#if (BARRIER_VERSION == 1)

    struct _sb     *sbp    = bp->sbp;

    pthread_mutex_lock( &sbp->lk );

    if ( sbp->runners == 1 ) {          /* last thread to reach barrier                 */
        if ( bp->limit != 1 ) {
                                        /* reset runner count and switch sub-barriers   */
            sbp->runners    = bp->limit;
            bp->sbp         = ( bp->sbp == &bp->sb[0] )? &bp->sb[1] : &bp->sb[0];

                                        /* wake up the waiters                          */
            pthread_cond_broadcast( &sbp->cv );
        }
    } else {
        sbp->runners--;                 /* one less runner                              */

        while ( sbp->runners != bp->limit )
            pthread_cond_wait( &sbp->cv, &sbp->lk );
    }

    pthread_mutex_unlock( &sbp->lk );

#elif (BARRIER_VERSION == 3)

	pthread_mutex_lock( &bp->lk );

	if ( bp->runners == 1 ) {           /* last thread to reach barrier                 */
		bp->generation++;               /* crank generation number                      */
		bp->runners	= bp->limit;        /* reset number of running threads              */

		pthread_cond_broadcast( &bp->cv );
	} else {
		int         cs_prev;
		int         cs_ignore;
		unsigned	genx	= bp->generation;

		bp->runners--;                  /* one less running thread                      */

		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs_prev );

                                        /* wait until the generation number changes.	*/
		while ( bp->generation == genx )
			pthread_cond_wait( &bp->cv, &bp->lk );

		pthread_setcancelstate( cs_prev, &cs_ignore );
	}

	pthread_mutex_unlock( &bp->lk );

#elif (BARRIER_VERSION == 6)

	SEM_TYPE	*wp;

	pthread_mutex_lock( &bp->lk );

	if ( bp->runners == 1 ) {           /* last thread to reach barrier                 */
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

		bp->waiters[--bp->runners]	= wp;
		wp->count			= 0;

		pthread_mutex_unlock( &bp->lk );
		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs_old );

		while ( SEM_WAIT( wp ) == EINTR )
            /* LINTED - empty loop	*/
			;
        
		pthread_setcancelstate( cs_old, &cs_ignore );
	}

#elif (BARRIER_VERSION == 7)
                                    /* First: get the lock.                             */
	pthread_mutex_lock( &bp->lk );

                                    /* Check the pre-conditions                         */
	assert( bp->runners >= 1 );
	assert( bp->limit  >= 1 );

	if ( bp->runners == 1 ) {       /* last thread to reach barrier                     */
		if ( bp->limit != 1 ) {
			waitlist_t	*hp;

                                    /* Traverse the list, posting to each waiter.       */
			for ( hp = bp->head; hp; hp = hp->next )
				sema_post( &hp->sem );

                                    /* Reset for the next iteration                     */
			bp->head	= NULL;
			bp->runners	= bp->limit;
		}

                                    /* Begin the next cycle                             */
		pthread_mutex_unlock( &bp->lk );
	} else {
		waitlist_t	waiter;         /* my element in the wait list                      */

		bp->runners--;              /* one less runner                                  */

		/*
		 * Initialize our semaphore (for blocking) and add it to the waiters list.
		 */

		sema_init( &waiter.sem, 0, bp->pshared, NULL );

		waiter.next	= bp->head;
		bp->head	= &waiter;

        /* 
         * Now that we're on the wait list, allow the next thread in.
         */

		pthread_mutex_unlock( &bp->lk );

                                    /* wait for the last thread to arrive.              */
		while ( ( status = sema_wait( &waiter.sem ) ) == EINTR )
			;

		assert( status == 0 );
		sema_destroy( &waiter.sem );
	}

#elif (BARRIER_VERSION == 8)

	SEM_TYPE	*wp;

	pthread_mutex_lock( &bp->lk );

	if ( bp->runners == 1 ) {       /* last thread to reach barrier                     */
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

#endif

	return( status );
}

/*
 * barrier_destroy - destroy a barrier variable.
 *
 */

	int
barrier_destroy( barrier_t *bp ) {
    int status  = 0;

#if (BARRIER_VERSION == 1)

    for ( i=0; i < 2; ++ i ) {
        if ( status = pthread_cond_destroy( &bp->sb[i].cv ) )
            return status;

        if ( status = pthread_mutex_destroy( &bp->sb[i].lk ) )
            return status;
    }

#elif (BARRIER_VERSION == 3)

    pthread_mutex_lock( &bp->lk );

	bp->limit       = 0;
	bp->runners     = 0;
	bp->generation	= 0;

	if ( status = pthread_cond_destroy( &bp->cv ) )
        return status;

	if ( status = pthread_mutex_unlock( &bp->lk ) )
        return status;

	if ( status = pthread_mutex_destroy( &bp->lk ) )
        return status;

#elif (BARRIER_VERSION == 6)

    pthread_mutex_lock( &bp->lk );

	{
		int	i		= 0;
		int	limit	= bp->limit - 1;

		for ( ; i < limit; ++i )
			bp->waiters[i]	= NULL;

		free( bp->waiters );
	}

	bp->limit	= 0;
	bp->runners	= 0;
	bp->waiters	= NULL;

	if ( status = pthread_mutex_unlock( &bp->lk ) )
        return status;

    status = pthread_mutex_destroy( &bp->lk );

#elif (BARRIER_VERSION == 7)

	assert( bp->head == NULL );
	assert( bp->runners == bp->limit );

	bp->limit	= 0;
	bp->runners	= 0;
	bp->pshared	= 0;

	status  = pthread_mutex_destroy( &bp->lk );

#elif (BARRIER_VERSION == 8)

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

	if ( status = pthread_mutex_unlock( &bp->lk ) )
        return status;

	status  = pthread_mutex_destroy( &bp->lk );

#endif

    return status;
}