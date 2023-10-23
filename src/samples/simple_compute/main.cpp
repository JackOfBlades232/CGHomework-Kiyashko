#include "simple_compute.h"

#include <vector>
#include <chrono>

#include <cstdlib>
#include <ctime>

#define MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))
#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))

void fill_random_vec(std::vector<float> &vec)
{
    for (uint32_t i = 0; i < vec.size(); ++i)
        vec[i] = (((float)std::rand() * (1.f / (float)RAND_MAX)) - (1.f * 0.5f)) * 2.f;
}

void print_vec(std::vector<float> &vec)
{
    for (uint32_t i = 0; i < vec.size(); ++i)
        std::cout << vec[i] << ' ';
    std::cout << '\n';
}

void perform_test(std::shared_ptr<ICompute> &app, int length)
{
    std::vector<float> input(length);
    std::vector<float> output(length);
    float gpu_compute_time;

    SimpleCompute *sc_app = (SimpleCompute *)app.get();
    sc_app->SetLength(length);
    sc_app->SetInOutPointers(&input, &output);
    sc_app->SetAdditionalPointers(&gpu_compute_time);

	fill_random_vec(input);

	std::chrono::steady_clock::time_point start, end;
	std::chrono::duration<float> elapsed_seconds;

	// CPU
	start = std::chrono::steady_clock::now();
	{
		constexpr int window_size = 7;
		constexpr float reduction_coeff = 1.f / window_size;

		// @NOTE: this is not the optimal implementation of a sliding window statistic
		for (int i = 0; i < input.size(); i++) {
			float win_sum = 0.f;

			for (int j = MAX(i - window_size/2, 0);
				 j < MIN(i - window_size/2 + window_size, input.size());
				 j++)
			{
				win_sum += input[j];                    
			}

			output[i] = input[i] - win_sum * reduction_coeff;
		}
	}
	end = std::chrono::steady_clock::now();
	elapsed_seconds = end - start;

	float sum = 0.f;
	for (auto v : output)
		sum += v;
	std::cout << "(CPU) Sum: " << sum << " | ";
	std::cout << "Reduction time: " << elapsed_seconds.count() << "s\n";

	// GPU
	start = std::chrono::steady_clock::now();
	{
		app->Execute();
	}
	end = std::chrono::steady_clock::now();
	elapsed_seconds = end - start;

	sum = 0.f;
	for (auto v : output)
		sum += v;
	std::cout << "(GPU compute) Sum: " << sum << " | ";
	std::cout << "Reduction time: " << elapsed_seconds.count() << "s (full setup and execution) or ";
	std::cout << gpu_compute_time << "s (just computation)\n";
}

int main()
{
    constexpr int VULKAN_DEVICE_ID = 0;

	const int lengths[] = { 1'000'000, 2'000'000, 5'000'000, 10'000'000, 20'000'000 };
	size_t num_tests    = sizeof(lengths) / sizeof(*lengths);

    std::srand(std::time(nullptr));

    std::shared_ptr<ICompute> app = std::make_unique<SimpleCompute>(0);
    if (!app)
    {
        std::cout << "Can't create render of specified type" << std::endl;
        return 1;
    }

    app->InitVulkan(nullptr, 0, VULKAN_DEVICE_ID);

	std::cout << '\n';
	for (int i = 0; i < num_tests; i++) {
        std::cout << "Test " << i + 1 << " (length " << lengths[i] << ")\n";
		perform_test(app, lengths[i]);
		std::cout << '\n';
	}

    return 0;
}
