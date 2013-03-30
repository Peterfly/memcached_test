#include <stdio.h>
#include <math.h>
#include <iostream> 
//#include <time.h>

//using namespace std;

#define SAMPLE_SIZE 100
#define NUM_STARS 1000

/** Predefine the parameters for key size variable. */
#define KEY_MU 30.7984
#define KEY_SIGMA 8.20449
#define KEY_XI 0.078688


/** Predefine the parameters for val size variable. */
#define VAL_MU 0
#define VAL_SIGMA 214.476
#define VAL_XI 0.348238

/** Predefine the parameters for inter-arrival time. */
#define INTER_MU 0
#define INTER_SIGMA 16.0292
#define INTER_XI 0.154971

void quicksort(double arr[], int beg, int end) {
    int i = beg, j = end;
    double pivot = arr[(beg + end)/2];

    while (i <= j) {
        while (pivot < arr[j]) j--;
        while (pivot > arr[i]) i++;

        if (i <= j) {
            double temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
            i++;
            j--;
        }
    }
    if (beg < j) {
        quicksort(arr, beg, j);
    }
    if (end > i) {
        quicksort(arr, i, end);
    }
}


/** The inverse CDF of GEV for key size. */
double key_gev(double p) {
    double base = -log(p);
    double result = pow(1/base, KEY_XI) - 1;
    result = result/KEY_XI * KEY_SIGMA + KEY_MU;
    return result;
}

/** The inverse CDF of Generalized Pareto for value size. */
double val_pareto(double p) {
    double temp = pow(1/(1 - p), VAL_XI) - 1;
    temp = temp/VAL_XI * VAL_SIGMA + VAL_MU;
    return temp;
}

/** The inverse CDF of Generalized Pareto for inter-arrival time. */
double inter_pareto(double p) {
    double temp = pow(1/(1 - p), INTER_XI) - 1;
    temp = temp/INTER_XI * INTER_SIGMA + INTER_MU;
    return temp;
}

double gen_rand() {
    return double(rand() % 10000000)/10000000.0;
}

void plot(double arr[]) {
    double increment = (arr[SAMPLE_SIZE - 1] - arr[0])/NUM_STARS;
    double cur = arr[0];
    int stars = 0;
    printf("(%.2f) - %.2f: ", cur, cur + increment);
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        if (arr[i] <= cur + increment) {
            stars++;
        } else {
            for (int j = 0; j < stars; j++) {
                printf("*");
            }
            printf("\n");
            cur += increment;
            stars = 0;
            printf("(%.2f) - %.2f: ", cur, cur + increment);
        }
    }
    for (int j = 0; j < stars; j++) {
        printf("*");
    }
    printf("\n");
}

void print_list(double arr[]) {
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        printf("%f\n", arr[i]);
    }
}

main()
{
    /** Set the seed of random number generator
     *  to the current time. */
    srand( (unsigned)time( NULL ) );
    printf("key size:");
    double arr[SAMPLE_SIZE] = {};
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        arr[i] = key_gev(gen_rand());
    }
    quicksort(arr, 0, SAMPLE_SIZE - 1);
    print_list(arr);
    printf("\n\n\n");
}
