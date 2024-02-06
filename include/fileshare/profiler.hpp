#pragma once


#ifndef ENABLE_PROFILER
#define ENABLE_PROFILER false
#endif // ENABLE_PROFILER

#if ENABLE_PROFILER

#include <mutex>
#include <string>

#define MEASURE_DURATION(name, description) fileshare::Profiler name(description)
#define MEASURE_CUMULATIVE_DURATION(name, description) fileshare::ProfilerCumulator name(description, false)
#define MEASURE_AVERAGE_DURATION(name, description) fileshare::ProfilerCumulator name(description, true)
#define MEASURE_ADD_CUMULATOR(name) fileshare::ProfilerCumulator::Instance name##_inst(name)

namespace fileshare
{
	class ProfilerCumulator
	{
	public:
		ProfilerCumulator(std::string name, bool average);
		~ProfilerCumulator();

		void append(double time)
		{
			std::lock_guard l(m);
			total += time;
			calls++;
		}

		class Instance
		{
		public:
			Instance(ProfilerCumulator& _parent);

			~Instance();

		private:
			const std::chrono::steady_clock::time_point start;
			ProfilerCumulator& parent;
		};

	private:
		const std::string description;
		double total = 0;
		int calls = 0;
		bool average;
		std::mutex m;
	};

	class Profiler
	{
	public:
		Profiler(std::string name);
		~Profiler();
		double getDuration();
		static void printDuration(const std::string& description, double duration);
		static std::string makeIndent();
	private:
		bool print = true;
		const std::chrono::steady_clock::time_point start;
		const std::string description;
	};

}

#else
#define MEASURE_DURATION(name, description)
#define MEASURE_CUMULATIVE_DURATION(name, description) 
#define MEASURE_AVERAGE_DURATION(name, description) 
#define MEASURE_ADD_CUMULATOR(name) 
#endif // ENABLE_PROFILER