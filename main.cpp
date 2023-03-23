/*******************************************************************************
 * Copyright (c) 2015 StreamComputing BV - All Rights Reserved                 *
 * www.streamcomputing.eu                                                      *
 *                                                                             *
 * This software contains source code provided by NVIDIA Corporation           *
 ******************************************************************************/

#include<stdlib.h>
#include<stdio.h>
#include<time.h>
#include<math.h>

#include "cl_util.h"

// CUDA SDK header that we use for time measurements
///////////////////////////////////////////////////////////////////////////////
#include "helper_timer.h"

///////////////////////////////////////////////////////////////////////////////
// Allocates memory for an array.
void AllocArray(int** pInput, int count)
{
    CHECK_NULL(pInput)

    *pInput = (int*)malloc(count * sizeof(int));
    CHECK_NULL(*pInput)
}

///////////////////////////////////////////////////////////////////////////////
// Initializes an array.
void InitArray(int* pInput, int count)
{
    CHECK_NULL(pInput)

    for (int i = 0; i < count; i++)
    {
        pInput[i] = rand();        
    }
}

///////////////////////////////////////////////////////////////////////////////
// Frees an image buffer.
void FreeArray(int** pInput)
{
    CHECK_NULL(pInput)

    if (*pInput)
    {
        free(*pInput);
        *pInput = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Compares two images pixel by pixel.
int CompareArrays(int* input1, int* input2, int count)
{
    CHECK_NULL(input1)
    CHECK_NULL(input2)

    float epsilon = 1.e-4f;
    for (int i = 0; i < count; i++)
        if (fabs(input1[i] - input2[i]) > epsilon)
        {
            printf("\nDifference found at offset [%d]: %.4f != %.4f",
                i, input1[i], input2[i]);
            return 0;
        }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// Launches the OpenCL kernel for doing color balance.
void SimpleFunction(int* input1, int* input2, int* output, int count)
{
    CHECK_NULL(input1);  
	CHECK_NULL(input2);  
    CHECK_NULL(output);

    for (int i = 0; i < count; i++)
    {
        output[i] = input1[i] + input2[i];
    }
    printf("\n%d + %d = %d", input1[0], input2[0], output[0]);
}

///////////////////////////////////////////////////////////////////////////////
// Launches the OpenCL kernel for doing color balance.
void SimpleFunctionOpenCL(cl_mem input1, cl_mem input2, cl_mem output, int count, cl_command_queue queue, cl_kernel simpleFunctionKernel)
{
    cl_int clError;
    cl_uint workDim;
    size_t globalWorkSize[3];
    size_t localWorkSize[3];

    // Set the kernel arguments
    clError = clSetKernelArg(simpleFunctionKernel, 0, sizeof(cl_mem), (void*)(&input1));
    CHECK_OCL_ERR("clSetKernelArg", clError);
	
	clError = clSetKernelArg(simpleFunctionKernel, 1, sizeof(cl_mem), (void*)(&input2));
    CHECK_OCL_ERR("clSetKernelArg", clError);

    clError = clSetKernelArg(simpleFunctionKernel, 2, sizeof(cl_mem), (void*)(&output));
    CHECK_OCL_ERR("clSetKernelArg", clError);

    clError = clSetKernelArg(simpleFunctionKernel, 3, sizeof(int), (void*)(&count));
    CHECK_OCL_ERR("clSetKernelArg", clError);

    // Specify work-item configuration
    workDim = 1;
    localWorkSize[0] = 384; //256;
    localWorkSize[1] = 0;
    localWorkSize[2] = 0;
    globalWorkSize[0] = ((count + localWorkSize[0] - 1) / localWorkSize[0]) * localWorkSize[0];
    globalWorkSize[1] = 0;
    globalWorkSize[2] = 0;

    // Add the kernel to the queue
    clError = clEnqueueNDRangeKernel(queue, simpleFunctionKernel, workDim, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);

    CHECK_OCL_ERR("clEnqueueNDRangeKernel", clError);
}

///////////////////////////////////////////////////////////////////////////////
// 
int main(void)
{
	int count = 40960000;
	
	int* input1 = NULL;    
	int* input2 = NULL;  
    int* output = NULL;
    int* gpuOutput = NULL;  
	
    StopWatchInterface *timer = NULL;
    StopWatchInterface *cpuProcTimer = NULL;
    StopWatchInterface *gpuProcTimer = NULL;
    float elapsedTimeInMs = 0;

    cl_platform_id platform = 0;
    cl_device_id device = 0;
    cl_context context = 0;
    cl_command_queue queue = 0;
 
    char* sourceCode = NULL;
    size_t sourceCodeLength = 0;

    cl_program program = 0;
    cl_kernel simpleFunctionKernel = 0;

    cl_mem dInput1 = 0; 
	cl_mem dInput2 = 0;
    cl_mem dOutput = 0;

    cl_int clError = 0;

    // Initializations
    sdkCreateTimer(&timer);
    sdkCreateTimer(&gpuProcTimer);
    sdkCreateTimer(&cpuProcTimer);
	
    srand((unsigned)time(NULL));

	AllocArray(&input1, count);
	AllocArray(&input2, count);
    // InitArray(input1, count);
	// InitArray(input2, count);
    
    AllocArray(&output, count);
    AllocArray(&gpuOutput, count);  

    // Check if OpenCL is supported and print info
    if (!PrintOpenCLInfo())
    {
        printf("\nNo OpenCL platform or device detected.");
        exit(EXIT_FAILURE);
    }

    // OpenCL initializations
    // Select an OpenCL platform and device
    SelectOpenCLPlatformAndDevice(&platform, &device);

    // Print the names of the selected platform and device
    printf("\nUsing platform "); PrintPlatformName(platform);
    printf(" and device "); PrintDeviceName(device);
    printf("\n");

    // Create an OpenCL context and command queue
    context = CreateOpenCLContext(platform, device);
    queue = CreateOpenCLQueue(device, context);

    // Load the OpenCL source code from file and create an OpenCL program.
    sourceCode = LoadOpenCLSourceFromFile("OpenCLKernels.cl", &sourceCodeLength);
    program = CreateAndBuildProgram(context, sourceCode, sourceCodeLength);

    // Create the OpenCL kernels
    simpleFunctionKernel = CreateKernel(program, "SimpleFunction");    

    // Create OpenCL buffers on device
    dInput1 = CreateDeviceBuffer(context, count * sizeof(int));
	dInput2 = CreateDeviceBuffer(context, count * sizeof(int));
    dOutput = CreateDeviceBuffer(context, count * sizeof(int));


for(int i = 0; i < 10 ; i++)
{
    printf("\n -------------------TEST %d----------------------", i + 1);
    InitArray(input1, count);
	InitArray(input2, count);
    // ________________________________________________________________________
    // Do processing on CPU
    // ________________________________________________________________________
    sdkStartTimer(&cpuProcTimer);

    // [CPU] Color Balance
    sdkStartTimer(&timer);
    {
        SimpleFunction(input1, input2, output, count);        
    }
    sdkStopTimer(&timer);
    sdkStopTimer(&cpuProcTimer);
    elapsedTimeInMs = sdkGetTimerValue(&timer);
    printf("\n%-55s %10.4f ms", "SimpleFunction (CPU)", elapsedTimeInMs);
    
    elapsedTimeInMs = sdkGetTimerValue(&cpuProcTimer);
    printf("\n%-55s %10.4f ms", "TOTAL CPU PROC TIME", elapsedTimeInMs);

    printf("\n");
    // ________________________________________________________________________
    // Do processing on GPU
    // ________________________________________________________________________
    sdkStartTimer(&gpuProcTimer);

    // [GPU] Copy data from host to device
    // ________________________________________________________________________
    sdkStartTimer(&timer);
    {
        CopyHostToDevice(input1, dInput1, count * sizeof(int), queue, CL_TRUE);
		CopyHostToDevice(input2, dInput2, count * sizeof(int), queue, CL_TRUE); 
    }
    sdkStopTimer(&timer);
    elapsedTimeInMs = sdkGetTimerValue(&timer);
    printf("\n%-55s %10.4f ms", "Transfer data to GPU ", elapsedTimeInMs);

    // [GPU] Color Balance
    // ________________________________________________________________________
    sdkStartTimer(&timer);
    {
        SimpleFunctionOpenCL(dInput1, dInput2, dOutput, count, queue, simpleFunctionKernel);
        clFinish(queue);
    }
    sdkStopTimer(&timer);
    elapsedTimeInMs = sdkGetTimerValue(&timer);
    printf("\n%-55s %10.4f ms", "SimpleFunction (GPU)", elapsedTimeInMs);
    // [GPU] Copy data from device to host
    // ________________________________________________________________________
    sdkStartTimer(&timer);
    {
        CopyDeviceToHost(dOutput, gpuOutput, count * sizeof(int), queue, CL_TRUE);        
    }
    sdkStopTimer(&timer);
    sdkStopTimer(&gpuProcTimer);
    elapsedTimeInMs = sdkGetTimerValue(&timer);
    printf("\n%-55s %10.4f ms", "Transfer data from GPU ", elapsedTimeInMs);
    
    printf("\n gpuOuput %d", gpuOutput[0]);

    printf("\nChecking the last OpenCL output...");
    if (!CompareArrays(output, gpuOutput, count))
        printf("\nIncorrect OpenCL output.");
    else
        printf("passed.");    

    elapsedTimeInMs = sdkGetTimerValue(&gpuProcTimer);
    printf("\n%-55s %10.4f ms", "TOTAL GPU PROC TIME", elapsedTimeInMs);
    printf("\n");
}
    // Free allocated resources
    FreeArray(&input1);  
	FreeArray(&input2); 
    FreeArray(&output);
    FreeArray(&gpuOutput);

    if (timer)
        sdkDeleteTimer(&timer);
    if (gpuProcTimer)
        sdkDeleteTimer(&gpuProcTimer);

    if (sourceCode)
        free(sourceCode);

    ReleaseDeviceBuffer(&dInput1);
	ReleaseDeviceBuffer(&dInput2);
    ReleaseDeviceBuffer(&dOutput);

    ReleaseKernel(&simpleFunctionKernel);    

    ReleaseProgram(&program);
    ReleaseOpenCLQueue(&queue);
    ReleaseOpenCLContext(&context);
}
