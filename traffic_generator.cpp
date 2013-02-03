#include <stdio.h>
#include <math.h>
#include <iostream> 
#include <fstream>

#include <time.h>
using namespace std;

//using namespace std;

#define SAMPLE_SIZE 100000
#define NUM_STARS 10000

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

void key_init(int num) {
    FILE *file = fopen("key", "a+");
    for (int i = 0 ; i < num; i++) {
        int len = (int) key_gev(gen_rand()) % 250;
        /*char *temp = (char *) malloc(250);
        for (int j = 0; j < len; j++) {
            temp[j] = ((i + 1) * len) % 26 + 65;
        }
        fprintf(file, "%s\n", temp);*/
        fprintf(file, "%d\n", len % 250);
    }
    fclose(file);
}

void value_init(int num) {
    FILE *file = fopen("value", "a+");
    for (int i = 0; i < num; i++) {
        int temp = (int) val_pareto(gen_rand());
        fprintf(file, "%d\n", temp % 5000);
    }
    fclose(file);
}

void inter_init(int num) {
    FILE *file = fopen("inter", "a+");
    for (int i = 0; i < num; i++) {
        int temp = (int) inter_pareto(gen_rand());
        fprintf(file, "%d\n", temp);
    }
    fclose(file);
}

main(int argc, char *argv[])
{
    /** Set the seed of random number generator
     *  to the current time. */
    srand( (unsigned)time( NULL ) );
    printf("Usage: [number] [type k/v/i]");
    if (argc != 3) {
        return 0;
    }
    // double arr[SAMPLE_SIZE] = {};
    int size = atoi(argv[1]);
    if (strcmp(argv[2], (char *)"a") == 0) {
        printf("print all\n");
        key_init(size);
        value_init(size);
        inter_init(size);
    }
    if (strcmp(argv[2], (char *)"k") == 0) {
        key_init(size);
        FILE *file = fopen("key", "a+");
        /*rewind(file);
        //char *temp = (char *) malloc(250);
        //while (fscanf(file, "%s", temp) != EOF)
        //printf("%s\n", temp);
        fclose(file);*/
    }
    else if (strcmp(argv[2], (char *)"v") == 0) {
        value_init(size);
        /*FILE *file = fopen("value", "a+");
        rewind(file);
        unsigned int temp;
        for (int j = 0; j < size; j++) {
            fscanf(file, "%x", &temp);
            printf("%x\n", temp);
        }
        fclose(file);*/
    }
    else if (strcmp(argv[2], (char *)"i") == 0) {
        inter_init(size);
        /*FILE *file = fopen("inter", "a+");
        rewind(file);
        unsigned int temp;
        for (int j = 0; j < size; j++) {
            fscanf(file, "%x", &temp);
            printf("%x\n", temp);
        }
        fclose(file);*/
    }

  

    /*FILE *file = fopen("key", "a+");
    rewind(file);*/

    /*char *temp = (char *) malloc(250);
    while (fscanf(file, "%s", temp) != EOF)
    printf("%s\n", temp);
    fclose(file);*/

    /*quicksort(arr, 0, SAMPLE_SIZE - 1);
    plot(arr);
    printf("\n\n\n");

    printf("value size\n\n");
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        arr[i] = val_pareto(gen_rand());
    }
    quicksort(arr, 0, SAMPLE_SIZE - 1);
    plot(arr);

    printf("\n\n\n");

    printf("inter-arrival\n\n");
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        arr[i] = inter_pareto(gen_rand());
    }
    quicksort(arr, 0, SAMPLE_SIZE - 1);
    plot(arr);

    for (int j = 0; j < 256; j++) {
        printf("%d: %c\n", j, char(j));
    }*/


    /*ofstream myfile;
    myfile.open ("example.txt");
    char *temp = (char *) malloc(20);
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        sprintf(temp, "%f\n", arr[i]);
        myfile << temp;
    }
    myfile.close();

    string line;
    ifstream inp("example.txt");
    if (inp.is_open())
    {
        while ( inp.good() )
        {
          getline (inp,line);
          cout << line << endl;
        }
        inp.close();
    }
    else cout << "Unable to open file"; 

    return 0;*/
}
