#include "profiler.hpp"

#if ENABLE_PROFILER
#include <iostream>

namespace fileshare
{
	static int profilerIndent = 0;
	static std::mutex profilerIndentMutex;

	Profiler::~Profiler()
	{
		if (print)
			Profiler::printDuration(description, getDuration());
		std::lock_guard m(profilerIndentMutex);
		profilerIndent--;
	}

	double Profiler::getDuration()
	{
		print = false;
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() /
			1000.0;
	}

	void Profiler::printDuration(const std::string& description, double duration)
	{
		std::cout << Profiler::makeIndent() << duration << "ms elapsed : " << description << std::endl;
	}

	std::string Profiler::makeIndent()
	{
		std::string str = " ";
		std::lock_guard m(profilerIndentMutex);
		for (int i = 0; i < profilerIndent; ++i)
			str = "--" + str;
		return str;
	}

	Profiler::Profiler(std::string _description) : start(std::chrono::steady_clock::now()),
		description(std::move(_description))
	{
		std::lock_guard m(profilerIndentMutex);
		profilerIndent++;
	}

	ProfilerCumulator::~ProfilerCumulator()
	{
		if (average)
			std::cout << Profiler::makeIndent() << total / calls << "ms elapsed in average : " << description <<
			" (" << calls <<
			" calls)" << std::endl;
		else
			std::cout << Profiler::makeIndent() << total << "ms elapsed in total : " << description << " (" <<
			calls << " calls)" << std::endl;
		std::lock_guard m(profilerIndentMutex);
		profilerIndent--;
	}

	ProfilerCumulator::Instance::Instance(ProfilerCumulator& _parent)
		: start(std::chrono::steady_clock::now()), parent(_parent)
	{
		std::lock_guard m(profilerIndentMutex);
		profilerIndent++;
	}

	ProfilerCumulator::Instance::~Instance()
	{
		parent.append(
			std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).
			count() / 1000.0);
		std::lock_guard m(profilerIndentMutex);
		profilerIndent--;
	}

	ProfilerCumulator::ProfilerCumulator(std::string _description, bool _average) :
		description(std::move(_description)), average(_average)
	{
		std::lock_guard m(profilerIndentMutex);
		profilerIndent++;
	}

}
#endif // ENABLE_PROFILER