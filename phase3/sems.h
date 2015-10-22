
#define MAX_SEMS 200

typedef struct semaphore semaphore;
struct semaphore {
 	int 		id;
 	int 		value;
 	int 		startingValue;
 	procPtr     blockedProcPtr;
 	int 		priv_mBoxID;
 	int 		mutex_mBoxID;
 	int			free_mBoxID;

 	// mutex (1,0)
 	// free (0,0)
 };