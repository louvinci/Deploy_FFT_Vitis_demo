#include "answer.hpp"
#include <sys/time.h>

#include "event_timer.hpp"
/*
void fft_test(std::string xclbin_path, DTYPE In_R[SIZE], DTYPE In_I[SIZE],DTYPE Out_R[SIZE],DTYPE Out_I[SIZE]) {
    EventTimer et;
    xf::common::utils_sw::Logger logger(std::cout, std::cerr);
    cl_int fail;

    //***************************************** Step1 platform related operations
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    //***************************************** Step2 Creating Context and Command Queue for selected Device


    et.add("OpenCL Initialization");

    cl::Context context(device, NULL, NULL, NULL, &fail);
    logger.logCreateContext(fail);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &fail); //TODO - 1
    logger.logCreateCommandQueue(fail);
    std::string devName = device.getInfo<CL_DEVICE_NAME>();

    cl::Program::Binaries xclBins = xcl::import_binary_file(xclbin_path);
    devices.resize(1);
    cl::Program program(context, devices, xclBins, NULL, &fail);
    logger.logCreateProgram(fail);
    cl::Kernel fft_kernel;
    fft_kernel = cl::Kernel(program, "fft", &fail);//The name "fft" should be same as your kernel name .xo
    et.finish();
    logger.logCreateKernel(fail);
    //***************************************** Step3 create device buffer and map dev buf to host buf
    for(int i = 0; i<1 ; i++){
    std::vector<cl_mem_ext_ptr_t> mext_o(4);
    mext_o[0] = {0, In_R, fft_kernel()};
    mext_o[1] = {1, In_I, fft_kernel()};
    mext_o[2] = {2, Out_R, fft_kernel()};
    mext_o[3] = {3, Out_I, fft_kernel()};

    cl::Buffer InR_buf, InI_buf, OutR_buf,OutI_buf;
    et.add("Map host buffers to OpenCL buffers");
    InR_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                            sizeof(DTYPE) * SIZE, &mext_o[0]);
    InI_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                            sizeof(DTYPE) * SIZE, &mext_o[1]);
    OutR_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                            sizeof(DTYPE) * SIZE, &mext_o[2]);
                            
    OutI_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                            sizeof(DTYPE) * SIZE, &mext_o[3]);
    et.finish();
    std::vector<cl::Memory> ob_in;
    std::vector<cl::Memory> ob_out;

    std::vector<cl::Event> events_write(1);
    std::vector<cl::Event> events_kernel(1);
    std::vector<cl::Event> events_read(1);

    ob_in.push_back(InR_buf);
    ob_in.push_back(InI_buf);

    struct timeval start_time, end_time;
    gettimeofday(&start_time, 0);

    et.add("Memory object migration enqueue");
    q.enqueueMigrateMemObjects(ob_in, 0, nullptr, &events_write[0]); //TODO start transfer, 0 host -> device 1 opposite

    clWaitForEvents(1, (const cl_event *)&events_write[0]);


    ob_out.push_back(OutR_buf);
    ob_out.push_back(OutI_buf);

    // launch kernel and calculate kernel execution time
    int j = 0;

    et.add("Set kernel arguments");
    fft_kernel.setArg(j++, InR_buf);
    fft_kernel.setArg(j++, InI_buf);
    fft_kernel.setArg(j++, OutR_buf);
    fft_kernel.setArg(j++, OutI_buf);

    et.add("OCL Enqueue task");
    q.enqueueTask(fft_kernel, &events_write, &events_kernel[0]);

    et.add("Wait for kernel to complete");

    clWaitForEvents(1, (const cl_event *)&events_kernel[0]);

    et.add("Read back computation results");
    q.enqueueMigrateMemObjects(ob_out, 1, &events_kernel, &events_read[0]);
    clWaitForEvents(1, (const cl_event *)&events_read[0]);
    et.finish();
    q.finish();

    if(i== 0)
    std::cout << "Kernel st execution time is: " <<start_time.tv_sec<<":"<< start_time.tv_usec  << std::endl;

    et.print();
    }
}
*/
void fft_test(std::string xclbin_path, DTYPE In_R[SIZE], DTYPE In_I[SIZE],DTYPE Out_R[SIZE],DTYPE Out_I[SIZE]) {
    EventTimer et;
    xf::common::utils_sw::Logger logger(std::cout, std::cerr);
    cl_int fail;

    //***************************************** Step1 platform related operations
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0]; //the device 0

    //***************************************** Step2 Creating Context and Command Queue for selected Device


    et.add("OpenCL Initialization");

    cl::Context context(device, NULL, NULL, NULL, &fail); //initial OpenCL environment
    logger.logCreateContext(fail);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &fail); //command queue
    logger.logCreateCommandQueue(fail);

    cl::Program::Binaries xclBins = xcl::import_binary_file(xclbin_path); //load the binary file
    devices.resize(1);
    cl::Program program(context, devices, xclBins, NULL, &fail); // pragram the fpga
    logger.logCreateProgram(fail);
    cl::Kernel fft_kernel;
    fft_kernel = cl::Kernel(program, "fft", &fail);//The name "fft" should be same as your kernel name .xo
    et.finish();
    logger.logCreateKernel(fail);
    //***************************************** Step3 create device buffer and map dev buf to host buf
 
    cl::Buffer InR_buf, InI_buf, OutR_buf,OutI_buf;
    et.add("Map host buffers to OpenCL buffers");
    InR_buf = cl::Buffer(context, static_cast<cl_mem_flags>(CL_MEM_READ_ONLY |
                                                     CL_MEM_USE_HOST_PTR),
                            sizeof(DTYPE) * SIZE, In_R);
    InI_buf = cl::Buffer(context, static_cast<cl_mem_flags>( CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY),
                            sizeof(DTYPE) * SIZE, In_I);
    OutR_buf = cl::Buffer(context, static_cast<cl_mem_flags>( CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY),
                            sizeof(DTYPE) * SIZE, Out_R);
                            
    OutI_buf = cl::Buffer(context, static_cast<cl_mem_flags>( CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY),
                            sizeof(DTYPE) * SIZE, Out_I);
    et.finish();
    
    int j = 0;
    et.add("Set kernel arguments");
    fft_kernel.setArg(j++, InR_buf);
    fft_kernel.setArg(j++, InI_buf);
    fft_kernel.setArg(j++, OutR_buf);
    fft_kernel.setArg(j++, OutI_buf);
    
    std::vector<cl::Event> events_write(1);
    std::vector<cl::Event> events_kernel(1);
    std::vector<cl::Event> events_read(1);


    struct timeval start_time, end_time;
    gettimeofday(&start_time, 0);

    et.add("Memory object migration enqueue");
    q.enqueueMigrateMemObjects({InR_buf,InI_buf}, 0, nullptr, &events_write[0]); //TODO start transfer, 0 host -> device 1 opposite

    clWaitForEvents(1, (const cl_event *)&events_write[0]);


    et.add("OCL Enqueue task");
    q.enqueueTask(fft_kernel, &events_write, &events_kernel[0]);

    et.add("Wait for kernel to complete");

    clWaitForEvents(1, (const cl_event *)&events_kernel[0]);

    et.add("Read back computation results");
    q.enqueueMigrateMemObjects({OutR_buf,OutI_buf}, 1, &events_kernel, &events_read[0]);
    clWaitForEvents(1, (const cl_event *)&events_read[0]);
    et.finish();
    q.finish();

    std::cout << "Kernel st execution time is: " <<start_time.tv_sec<<":"<< start_time.tv_usec  << std::endl;

    et.print();

}
