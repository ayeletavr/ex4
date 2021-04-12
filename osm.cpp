#include "osm.h"
#include <iostream>
#include <sys/time.h>
#include <math.h>
using namespace std;

// ---consts and defines---
#define ERROR -1
#define ADD_INSTRUCTION 1
#define EMPTY_FUNC 2
#define TRAP 3
const unsigned int UNROLLING_COEFF = 5;

void emptyFunc() {}

static double timeMeasurement(unsigned int operation, unsigned int iterations) {
    /** Time measurement test for one of three operations (addition instruction, empty function or
     * a trap.
     */
     if (iterations == 0) {
         return ERROR;
     }

     timeval before, after;
     double time_measured, time_per_iter;

     if (operation == ADD_INSTRUCTION) {
         gettimeofday(&before, NULL);
         unsigned int x;
         x = 0;
         for (unsigned int i = 0; i < iterations; i += 1) {
             x += 1;
             x += 1;
             x += 1;
             x += 1;
             x += 1;
         }
         gettimeofday(&after, NULL);
     }

     if (operation == EMPTY_FUNC) {
         gettimeofday(&before, NULL);
         for (unsigned int i = 0; i < iterations; i += 1) {
             emptyFunc();
             emptyFunc();
             emptyFunc();
             emptyFunc();
             emptyFunc();
         }
     }

     if (operation == TRAP) {
         OSM_NULLSYSCALL;
         OSM_NULLSYSCALL;
         OSM_NULLSYSCALL;
         OSM_NULLSYSCALL;
         OSM_NULLSYSCALL;
     }

     time_measured = ((after.tv_sec - before.tv_sec) / UNROLLING_COEFF);
//     time_per_iter = time_measured / iterations;
     return time_measured;

}

double osm_operation_time(unsigned int iterations) {
    /** Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
   return timeMeasurement(ADD_INSTRUCTION, iterations);
}

double osm_function_time(unsigned int iterations) {
    /** Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
    return timeMeasurement(EMPTY_FUNC, iterations);
}

double osm_syscall_time(unsigned int iterations) {
    /** Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
    return timeMeasurement(TRAP, iterations);
}

double time_per_iteration(double time, unsigned int iterations) {
    return time / iterations;
}

int main(int argc, char *argv[])
{
    int iterations;
    int counter = 10;

    for (int i = 0; i < 5; i++) {
        cout<< "Enter num of iterations: \n";
        cin >> iterations;
        cout << "iterations: " << iterations << "\n";
        cout << "addition: " << osm_operation_time(iterations) << "\n"
             << "function: " << osm_function_time(iterations) << "\n" <<
             "trap: " << osm_syscall_time(iterations) << "\n";

        cout << "addition per iter: " << time_per_iteration(osm_operation_time(iterations) ,iterations) << "\n"
             << "function per iter: " << time_per_iteration(osm_function_time(iterations), iterations) << "\n" <<
             "trap per iter: " << time_per_iteration(osm_syscall_time(iterations), iterations) << "\n";
    }

//    cout<< "Enter num of iterations: \n";
//    cin >> iterations;
//    cout << "addition: " << osm_operation_time(iterations) << "\n"
//    << "function: " << osm_function_time(iterations) << "\n" <<
//    "trap: " << osm_syscall_time(iterations) << "\n";
//
//    cout << "addition per iter: " << time_per_iteration(osm_operation_time(iterations) ,iterations) << "\n"
//         << "function per iter: " << time_per_iteration(osm_function_time(iterations), iterations) << "\n" <<
//         "trap per iter: " << time_per_iteration(osm_syscall_time(iterations), iterations) << "\n";


    return EXIT_SUCCESS;
}