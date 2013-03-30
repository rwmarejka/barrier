#if !defined(BARRIER_INCLUDED)
#define	BARRIER_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

/* Feature Test Macros      */

#define	_POSIX_PTHREAD_SEMANTICS

#define BARRIER_SUBBARRIER  (1)
#define BARRIER_GENERATION  (3)
#define BARRIER_SEMAPHORE   (8)

#if !defined(BARRIER_VERSION)
#   define BARRIER_VERSION  BARRIER_SEMAPHORE
#endif
 
/* Include Files            */

#include <pthread.h>

#if (BARRIER_VERSION == BARRIER_SEMAPHORE)
#   include <semaphore.h>
#endif

/* Constants and Macros     */
/* Data Definitions         */

typedef struct {
    unsigned        limit;          /* maximum number of waiters        */
    unsigned        runners;        /* current number of runners        */
    pthread_mutex_t lk;             /* lock to protect it all           */
#if (BARRIER_VERSION == BARRIER_SUBBARRIER)
    struct _sb {
        pthread_cond_t  cv;         /* cv for waiters at barrier        */
        pthread_mutex_t lk;         /* mutex for waiters at barrier     */
        int             runners;    /* number of running threads        */
    } sb[2];
    struct _sb      *sbp;           /* current sub-barrier              */
#elif (BARRIER_VERSION == BARRIER_GENERATION)
    pthread_cond_t	cv;             /* rendezvous point                 */
	unsigned        generation;     /* barrier usage generation         */
#elif (BARRIER_VERSION == BARRIER_SEMAPHORE)
    sem_t           **waiters;      /* array of waiters                 */
#endif
} barrier_t;

/* External References		*/

extern	int	barrier_init( barrier_t *, unsigned );
extern	int	barrier_wait( barrier_t * );
extern	int	barrier_destroy( barrier_t * );

#if defined(__cplusplus)
}
#endif
#endif	/* BARRIER_INCLUDED	*/
