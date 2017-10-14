#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The nsSem is used as a a lock.
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
static struct lock *ilock;
static struct cv *icv;
static struct array *cars;

typedef struct car {
  Direction origin;
  Direction dest;
} car;

int allowed(car *first, car *inside);
bool right_turn(car* c);
// bool left_turn(car* v);
void wait(car *c);

bool right_turn(car* c) {
  if (((c->origin == west) && (c->dest == south)) ||
      ((c->origin == east) && (c->dest == north)) ||
      ((c->origin == south) && (c->dest == east))  ||
      ((c->origin == north) && (c->dest == west))) {
    return true;
  }
  return false;
}

// bool left_turn(car* v) {
//   if (((v->origin == west) && (v->dest == north)) ||
//       ((v->origin == east) && (v->dest == south)) ||
//       ((v->origin == south) && (v->dest == west)) ||
//       ((v->origin == north) && (v->dest == east))) {
//     return true;
//   }
//   return false;
// }

int allowed(car* first, car* inside) {
  if (first->origin == inside->origin) {
    return 1;
  }
  else if ((first->origin == inside->dest) && (first->dest == inside->origin)) {
    return 1;
  }
  else if ((right_turn(first) || right_turn(inside)) && (first->dest != inside->dest)) {
    return 1;
  }
  // else if (left_turn(first) && left_turn(inside)) {
  //   if ((first->dest == east || first->dest == west) &&
  //       (inside->dest == east || inside->dest == west)) {
  //     return 1;
  //   }
  //   else if ((first->dest == north || first->dest == south) &&
  //           (inside->dest == north|| inside->dest == south)) {
  //     return 1;
  //   }
  // }
  return 0;
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

  ilock = lock_create("ilock");
  if (ilock == NULL) {
    panic("could not create intersection lock");
  }
  icv = cv_create("icv");
  if (icv == NULL) {
    panic("could not create intersection condition variable");
  }
  cars = array_create();
  if (cars == NULL) {
    panic("could not create intersection cars queue");
  }
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */

void wait (car *c) {
  int size = array_num(cars);
  for (int i = 0; i < size; i++) {
    if (!allowed(c, array_get(cars, i))) {
      cv_wait(icv, ilock);
      return wait(c);
    }
  }  
  array_add(cars, c, NULL);
}

void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(ilock != NULL);
  lock_destroy(ilock);
  KASSERT(icv != NULL);
  cv_destroy(icv);
  KASSERT(cars != NULL);
  array_destroy(cars);
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
  /* replace this default implementation with your own implementation */
  
  lock_acquire(ilock);
  car *first = kmalloc(sizeof(struct car));
  first->origin = origin;
  first->dest = destination;

  // int size = array_num(cars);
  // for (int i = 0; i < size; i++) {
  //   if (!allowed(first, array_get(cars, i))) {
  //     cv_wait(icv, ilock);
  //     continue;
  //   }
  // }
  // array_add(cars, first, NULL);

  wait(first);
  lock_release(ilock);
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

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  
  lock_acquire(ilock);
  int size = array_num(cars);
  car *ith = NULL;
  for (int i = 0; i < size; i++) {
    ith = array_get(cars, i);
    if ((ith->origin == origin) && (ith->dest == destination)) {
      array_remove(cars, i);
      cv_broadcast(icv, ilock);
      break;
    }
  }
  lock_release(ilock);
}

