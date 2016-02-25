// If using gcc, compile with flags -fopenmp -libgomp
#include <stdio.h>
#include "omp.h"

int
main(){
        int a,b=3;

#pragma omp parallel private(a) shared(b) num_threads(8)
        {
                a = omp_get_thread_num();
#pragma omp critical
                {
                        b++;
                }
                fprintf(stdout, "Thread id=%i, b=%i\n", a, b);
        }
        return 0;
}
