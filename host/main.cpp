#include "utils.hpp"
#include "answer.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <set>
#include "xcl2.hpp"
#include "logger.hpp"




struct Rmse
{
    int num_sq;
    float sum_sq;
    float error;
    
    Rmse(){ num_sq = 0; sum_sq = 0; error = 0; }
    
    float add_value(float d_n)
    {
        num_sq++;
        sum_sq += (d_n*d_n);
        error = sqrtf(sum_sq / num_sq);
        return error;
    }
    
};
Rmse rmse_R,  rmse_I;

int main(int argc, const char* argv[]) {
    std::cout << "\n-------------------fft----------------\n";
    // cmd parser
    ArgParser parser(argc, argv);
    std::string tmpStr;
    if (!parser.getCmdOption("--xclbin", tmpStr)) {
        std::cout << "ERROR:xclbin path is not set!\n";
        return 1;
    }
    std::string xclbin_path = tmpStr;

    if (!parser.getCmdOption("--scale", tmpStr)) {
        std::cout << "ERROR: scale is not set" << std::endl;
    }

    DTYPE* In_R = aligned_alloc<DTYPE>(SIZE * sizeof(DTYPE));
    DTYPE* In_I = aligned_alloc<DTYPE>(SIZE * sizeof(DTYPE));

    DTYPE* Out_R = aligned_alloc<DTYPE>(SIZE * sizeof(DTYPE));
    DTYPE* Out_I = aligned_alloc<DTYPE>(SIZE * sizeof(DTYPE));

    DTYPE gold_R, gold_I;
    int index=0;
    
    for(int i=0; i<SIZE; i++)	{
		In_R[i] = i;
		In_I[i] = 0.0;
		Out_R[i]=i;
		Out_I[i]=1;
	}
 
    //**Step2 start kernel execution
    
    
    struct timeval start_time, end_time,s_time2;
    //gettimeofday(&start_time, 0);
    fft_test(xclbin_path,In_R, In_I, Out_R, Out_I);
    gettimeofday(&end_time, 0);

    std::cout << "Kernel ed execution time is: " <<end_time.tv_sec<<":"<< end_time.tv_usec << " ms" << std::endl;

    //std::cout << "End to End execution time is: " << tvdiff(&start_time, &end_time) / 1000UL << " ms" << std::endl;

    //**Step3 Check Results with golden output
    FILE * fp = fopen("out.gold.dat","r");
    int dcnt = 0;
    for(int i=0; i<SIZE; i++)
    {
        fscanf(fp, "%d %f %f", &index, &gold_R, &gold_I);
        rmse_R.add_value(Out_R[i] - gold_R);
        rmse_I.add_value(Out_I[i] - gold_I);
    }
    fclose(fp);

    printf("----------------------------------------------\n");
    printf("   RMSE(R)           RMSE(I)\n");
    printf("%0.15f %0.15f\n", rmse_R.error, rmse_I.error);
    printf("----------------------------------------------\n");
    
    if (rmse_R.error > 1 || rmse_I.error > 1 ) {
        fprintf(stdout, "*******************************************\n");
        fprintf(stdout, "FAIL: Output DOES NOT match the golden output\n");
        fprintf(stdout, "*******************************************\n");
        return 1;
    } else {
        fprintf(stdout, "*******************************************\n");
        fprintf(stdout, "PASS: The output matches the golden output!\n");
        fprintf(stdout, "*******************************************\n");
        return 0;
    }
}
