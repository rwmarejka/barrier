/*
 *
 */

/* Feature Test Macros	*/

#define BARRIER_VERSION     (8)

/* Include Files	*/

#include <barrier.h>
#include <assert.h>
#include <stdio.h>

/* Constants & Macros	*/

/* Data Declarations	*/

/* External Declarations	*/

#if (BARRIER_VERSION == 3) || (BARRIER_VERSION == 8)
static	const unsigned  BARRIER_LIMIT_DEFAULT	= 1;
static	const int       BARRIER_PSHARED_DEFAULT	= PTHREAD_PROCESS_PRIVATE;
#endif

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

    {
        int i;

        for ( i=0; i < 2; ++i ) {
            struct _sb  *sbp    = &( bp->sb[i] );

            sbp->runners    = count;

            if ( status = pthread_mutex_init( &sbp->lk, NULL ) )
                return status;

            if ( status = pthread_cond_init( &sbp->cv, NULL ) )
                return status;
        }
    }
    
#elif (BARRIER_VERSION == 3)
    
    if ( status = pthread_mutex_init( &bp->lk, NULL ) )
        return status;

	if ( status = pthread_cond_init( &bp->cv, NULL ) )
        return status;

	bp->limit	= count;
	bp->runners	= count;
	bp->generation	= 0;

#elif (BARRIER_VERSION == 8)
    
    if ( bp->waiters = (sem_t **) calloc( count, sizeof( sem_t * ) ) ) {
        unsigned    i;

		pthread_mutex_init( &bp->lk, NULL );

        for ( i=0; i < count; ++i ) {
            char    buf[BUFSIZ];

            snprintf( buf, BUFSIZ, "/barrier/%d/%u", getpid(), i );
            bp->waiters[i]  = sem_open( buf, O_CREAT | O_EXCL );
        }

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

#elif (BARRIER_VERSION == 8)

	sem_t   *wp;

	pthread_mutex_lock( &bp->lk );

	if ( bp->runners == 1 ) {       /* last thread to reach barrier                     */
		int		i	= 1;
		int		limit	= bp->limit;
		sem_t	**wlp	= bp->waiters;

		/*
		 * Run the list of waiters, waking each using a SEM_POST. Then
		 * reset the current number of runners to the limit value.
		 */

		for ( ; i < limit; ++i )
			sem_post( *(++wlp) );

        bp->runners	= bp->limit;

		pthread_mutex_unlock( &bp->lk );
	} else {
		int	cs_old;
		int	cs_ignore;

        wp  = bp->waiters[ --bp->runners ];

		pthread_mutex_unlock( &bp->lk );
		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs_old );

		while ( sem_wait( wp ) == EINTR )
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
    int i;

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

#elif (BARRIER_VERSION == 8)

    pthread_mutex_lock( &bp->lk );

	{
		unsigned	i;
		unsigned	limit	= bp->limit;

        for ( i=0; i < limit; ++i )
            sem_close( bp->waiters[i] );

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
