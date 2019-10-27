#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock *intersection_lock;
static struct cv * North_cv;
static volatile int North_wait = 0;
static struct cv * South_cv;
static volatile int South_wait = 0;
static struct cv * East_cv;
static volatile int East_wait = 0;
static struct cv * West_cv;
static volatile int West_wait = 0;



static struct array * in_intersection;

struct Car {
  Direction origin;
  Direction destination;
};

bool right_turn(struct Car *car);
bool traffic_rules(struct Car * car_1,struct  Car * car_2);
struct cv *maxWait(void);

struct cv * maxWait(void) {
  if (North_wait > South_wait && North_wait > East_wait && North_wait > West_wait) {
    North_wait = 0;
    return North_cv;
  }
  else if (South_wait > North_wait && South_wait > East_wait && South_wait > West_wait) {
    South_wait = 0;
    return South_cv;
  }
  else if (East_wait > South_wait && East_wait > North_wait && East_wait > West_wait) {
    East_wait = 0;
    return East_cv;
  } 
  else if (West_wait > South_wait && West_wait > North_wait && West_wait > East_wait) {
    West_wait = 0;
    return West_cv;
  }
  else {
    North_wait = 0;
    return North_cv;
  }
}


bool right_turn(struct Car *car) {
  if (((car->origin == west) && (car->destination == south)) ||
      ((car->origin == south) && (car->destination == east)) ||
      ((car->origin == east) && (car->destination == north)) ||
      ((car->origin == north) && (car->destination == west))) {
    return true;
  } 
  else {
    return false;
  }
}

bool traffic_rules(struct Car * car_1,struct  Car * car_2) {
  if (car_1->origin == car_2->origin || 
  ((car_1->origin == car_2->destination) && (car_1->destination == car_2->origin)) ||
  ((car_1->destination != car_2->destination) && (right_turn(car_1) ||right_turn(car_2)))){
    return true;
  }
  else {
    return false;
  }
}

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
  North_cv = cv_create("North");
  South_cv = cv_create("South");
  East_cv = cv_create("East");
  West_cv = cv_create("West");
  intersection_lock = lock_create("intersection_lock");
  in_intersection = array_create();
  array_init(in_intersection);
  // if (North_cv == NULL || South_cv == NULL || East_cv == NULL || West_cv = NULL) {
  //   panic("could not create intersection cv");
  // }
  if (intersection_lock == NULL) {
    panic("could not create intersection lock");
  }
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(intersection_lock != NULL);
  lock_destroy(intersection_lock);
  KASSERT(North_cv != NULL);
  cv_destroy(North_cv);
  KASSERT(South_cv != NULL);
  cv_destroy(South_cv);
  KASSERT(East_cv != NULL);
  cv_destroy(East_cv);
  KASSERT(West_cv != NULL);
  cv_destroy(West_cv);
  array_destroy(in_intersection);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  struct Car *car = kmalloc(sizeof(struct Car));
  car->origin = origin;
  car->destination = destination;
  lock_acquire(intersection_lock);
  for (unsigned int i = 0; i < array_num(in_intersection); i++) { //in_intersection may have changed after the car wakes up again
    bool go_in_traffic = traffic_rules(car, array_get(in_intersection,i));
    if (!go_in_traffic) {
      while (array_num(in_intersection) > 0){
      if (origin == north) {
        North_wait++;
         cv_wait(North_cv, intersection_lock); // Can go to sleep multiple times...

        }
        else if (origin == south) {
          South_wait++;
          cv_wait(South_cv, intersection_lock);
        } 
        else if (origin == east) {
          East_wait++;
          cv_wait(East_cv, intersection_lock);
        }
        else {
          West_wait++;
          cv_wait(West_cv, intersection_lock);
        }
        
    }
    break;
    }
  }
  array_add(in_intersection, car, NULL);
  lock_release(intersection_lock);
}

/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

//have a fucntion to decide which cv wait channel to wake up next

void
intersection_after_exit(Direction origin, Direction destination) 
{
  lock_acquire(intersection_lock);
  struct Car * car = kmalloc(sizeof(struct Car));
  car->origin = origin;
  car->destination = destination;
  //Find which car is leaving
  for (unsigned int i = 0; i < array_num(in_intersection); i ++){
    struct Car *exit_car = array_get(in_intersection, i);
    if (exit_car->origin == car->origin && exit_car->destination == car->destination) {
      array_remove(in_intersection, i);
      struct cv *wake = maxWait();
      cv_broadcast(wake, intersection_lock);
      break;
    }
  }

  lock_release(intersection_lock); 
}