#include <libutils/misc.h>
#include <libutils/timer.h>
#include <libutils/fast_random.h>
#include <libgpu/context.h>
#include <libgpu/shared_device_buffer.h>

#include "cl/matrix_transpose_cl.h"

#include <vector>
#include <iostream>
#include <stdexcept>


int main(int argc, char **argv)
{
    gpu::Device device = gpu::chooseGPUDevice(argc, argv);

    gpu::Context context;
    context.init(device.device_id_opencl);
    context.activate();

    int benchmarkingIters = 10;
    unsigned int M = 1024;
    unsigned int K = 1024;

    // unsigned int M = 16;
    // unsigned int K = 16;

    std::vector<float> as(M*K, 0);
    std::vector<float> as_t(M*K, 0);

    FastRandom r(M+K);
    for (unsigned int i = 0; i < as.size(); ++i) {
        as[i] = r.nextf();
        // as[i] = i;
    }
    std::cout << "Data generated for M=" << M << ", K=" << K << "!" << std::endl;

    
    gpu::gpu_mem_32f as_gpu, as_t_gpu;
    as_gpu.resizeN(M*K);
    as_t_gpu.resizeN(K*M);

    as_gpu.writeN(as.data(), M*K);

    ocl::Kernel matrix_transpose_kernel(matrix_transpose, matrix_transpose_length, "matrix_transpose");
    matrix_transpose_kernel.compile();

    {
        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            unsigned int square_size = 32;
            unsigned int global_work_size_x = gpu::divup(M, square_size) * square_size;
            unsigned int global_work_size_y = gpu::divup(K, square_size) * square_size;
            matrix_transpose_kernel.exec(
                gpu::WorkSize(
                    square_size, square_size,
                    global_work_size_x, global_work_size_y
                ), 
                as_gpu, as_t_gpu, M, K);

            t.nextLap();
        }
        std::cout << "GPU: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "GPU: " << M*K/1000.0/1000.0 / t.lapAvg() << " millions/s" << std::endl;
    }

    as_t_gpu.readN(as_t.data(), M*K);

    // Проверяем корректность результатов
    // for (int j = 0; j < M; ++j){
    //     for (int i = 0; i < K; ++i)
    //         std::cout << as_t[j * K + i] << ' ';
    //     std::cout << std::endl;
    // }
    for (int j = 0; j < M; ++j) {
        for (int i = 0; i < K; ++i) {
            float a = as[j * K + i];
            float b = as_t[i * M + j];
            if (a != b) {
                std::cerr << "Not the same!" << std::endl;
                return 1;
            }
        }
    }

    return 0;
}