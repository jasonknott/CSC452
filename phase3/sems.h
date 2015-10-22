
#define MAX_SEMS 200

typedef struct semaphore semaphore;
struct semaphore {
 	int 		id;
 	int 		value;
 	procPtr     blockedProcPtr;
 	int 		mBoxID;
 };