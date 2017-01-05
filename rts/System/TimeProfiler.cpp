/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include <climits>
#include <cstring>

#include "System/TimeProfiler.h"
#include "System/GlobalRNG.h"
#include "System/Log/ILog.h"
#include "System/Threading/SpringThreading.h"

#ifdef THREADPOOL
	#include "System/ThreadPool.h"
#endif

static spring::mutex m;
static spring::unordered_map<int, std::string> hashToName;
static spring::unordered_map<int, int> refCounters;



static unsigned hash_(const std::string& s)
{
	unsigned hash = s.size();
	for (std::string::const_iterator it = s.begin(); it != s.end(); ++it) {
		hash += *it;
		hash ^= (hash << 7) | (hash >> (sizeof(hash) * CHAR_BIT - 7));
	}
	return hash;
}

static unsigned hash_(const char* s)
{
	unsigned hash = 0;
	for (size_t i = 0; ; ++i) {
		if (s[i]) {
			hash += s[i];
			hash ^= (hash << 7) | (hash >> (sizeof(hash) * CHAR_BIT - 7));
		} else {
			hash += (unsigned) i;
			break;
		}
	}
	return hash;
}


BasicTimer::BasicTimer(const std::string& timerName)
	: nameHash(hash_(timerName))
	, startTime(spring_gettime())

	, name(timerName)
{
	const auto iter = hashToName.find(nameHash);

	if (iter == hashToName.end()) {
		hashToName.insert(std::pair<int, std::string>(nameHash, timerName)).first;
	} else {
#ifdef DEBUG
		if (iter->second != timerName) {
			LOG_L(L_ERROR, "Timer hash collision: %s <=> %s", timerName.c_str(), iter->second.c_str());
			assert(false);
		}
#endif
	}
}


BasicTimer::BasicTimer(const char* timerName)
	: nameHash(hash_(timerName))
	, startTime(spring_gettime())

	, name(timerName)
{
	const auto iter = hashToName.find(nameHash);

	if (iter == hashToName.end()) {
		hashToName.insert(std::pair<int, std::string>(nameHash, timerName)).first;
	} else {
#ifdef DEBUG
		if (iter->second != timerName) {
			LOG_L(L_ERROR, "Timer hash collision: %s <=> %s", timerName, iter->second.c_str());
			assert(false);
		}
#endif
	}
}

spring_time BasicTimer::GetDuration() const
{
	return spring_difftime(spring_gettime(), startTime);
}



ScopedTimer::ScopedTimer(const std::string& name, bool autoShow)
	: BasicTimer(name)
	, autoShowGraph(autoShow)

{
	refsIterator = refCounters.find(nameHash);

	if (refsIterator == refCounters.end())
		refsIterator = refCounters.insert(std::pair<int, int>(nameHash, 0)).first;

	++(refsIterator->second);
}

ScopedTimer::ScopedTimer(const char* name, bool autoShow)
	: BasicTimer(name)
	, autoShowGraph(autoShow)

{
	refsIterator = refCounters.find(nameHash);

	if (refsIterator == refCounters.end())
		refsIterator = refCounters.insert(std::pair<int, int>(nameHash, 0)).first;

	++(refsIterator->second);
}

ScopedTimer::~ScopedTimer()
{
	if (--(refsIterator->second) == 0) {
		profiler.AddTime(GetName(), startTime, GetDuration(), autoShowGraph, false);
	}
}



ScopedOnceTimer::ScopedOnceTimer(const char* timerName)
	: startTime(spring_gettime())
	, name(timerName)
{
}

ScopedOnceTimer::ScopedOnceTimer(const std::string& timerName)
	: startTime(spring_gettime())
	, name(timerName)
{
}

ScopedOnceTimer::~ScopedOnceTimer()
{
	LOG("%s: %i ms", name.c_str(), int(GetDuration().toMilliSecsi()));
}

spring_time ScopedOnceTimer::GetDuration() const
{
	return spring_difftime(spring_gettime(), startTime);
}



ScopedMtTimer::ScopedMtTimer(const std::string& timerName, bool autoShow)
	// can not call BasicTimer's other ctor, accesses global map
	// collisions for MT timers do not need to be checked anyway
	: BasicTimer(spring_gettime())
	, autoShowGraph(autoShow)
{
	name = timerName;
}

ScopedMtTimer::ScopedMtTimer(const char* timerName, bool autoShow)
	: BasicTimer(spring_gettime())
	, autoShowGraph(autoShow)
{
	name = timerName;
}

ScopedMtTimer::~ScopedMtTimer()
{
	profiler.AddTime(GetName(), startTime, GetDuration(), autoShowGraph, true);
}



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTimeProfiler::CTimeProfiler():
	lastBigUpdate(spring_gettime()),
	currentPosition(0)
{
#ifdef THREADPOOL
	profileCore.resize(ThreadPool::GetMaxThreads());
#endif
}

CTimeProfiler::~CTimeProfiler()
{
	std::unique_lock<spring::mutex> ulk(m, std::defer_lock);
	while (!ulk.try_lock()) {}
}

CTimeProfiler& CTimeProfiler::GetInstance()
{
	static CTimeProfiler tp;
	return tp;
}

void CTimeProfiler::Update()
{
	//FIXME non-locking threadsafe
	std::unique_lock<spring::mutex> ulk(m, std::defer_lock);
	while (!ulk.try_lock()) {}

	++currentPosition;
	currentPosition &= TimeRecord::frames_size-1;
	for (auto& pi: profile) {
		pi.second.frames[currentPosition] = spring_notime;
	}

	const spring_time curTime = spring_gettime();
	const float timeDiff = spring_diffmsecs(curTime, lastBigUpdate);
	if (timeDiff > 500.0f) // twice every second
	{
		for (auto& pi: profile) {
			auto& p = pi.second;
			p.percent = spring_tomsecs(p.current) / timeDiff;
			p.current = spring_notime;
			p.newLagPeak = false;
			p.newPeak = false;
			if(p.percent > p.peak) {
				p.peak = p.percent;
				p.newPeak = true;
			}
		}
		lastBigUpdate = curTime;
	}

	if (curTime.toSecsi() % 6 == 0) {
		for (auto& pi: profile) {
			auto& p = pi.second;
			p.maxLag *= 0.5f;
		}
	}
}

float CTimeProfiler::GetPercent(const char* name)
{
	std::unique_lock<spring::mutex> ulk(m, std::defer_lock);
	while (!ulk.try_lock()) {}

	return profile[name].percent;
}

void CTimeProfiler::AddTime(
	const std::string& name,
	const spring_time startTime,
	const spring_time deltaTime,
	const bool showGraph,
	const bool threadTimer
) {
#ifdef THREADPOOL
	if (threadTimer)
		profileCore[ThreadPool::GetThreadNum()].emplace_back(startTime, spring_gettime());
#endif

	auto pi = profile.find(name);

	if (pi != profile.end()) {
		// profile already exists
		//FIXME use atomic ints
		auto& p = pi->second;
		p.total   += deltaTime;
		p.current += deltaTime;
		p.frames[currentPosition] += deltaTime;

		if (p.maxLag < deltaTime.toMilliSecsf()) {
			p.maxLag     = deltaTime.toMilliSecsf();
			p.newLagPeak = true;
		}
	} else {
		std::unique_lock<spring::mutex> ulk(m, std::defer_lock);
		while (!ulk.try_lock()) {}

		// create a new profile
		auto& p = profile[name];
		p.total   = deltaTime;
		p.current = deltaTime;
		p.maxLag  = deltaTime.toMilliSecsf();
		p.percent = 0;
		memset(p.frames, 0, TimeRecord::frames_size * sizeof(unsigned));

		static CGlobalUnsyncedRNG rand;
		rand.Seed(spring_tomsecs(spring_gettime()));

		p.color.x = rand.NextFloat();
		p.color.y = rand.NextFloat();
		p.color.z = rand.NextFloat();
		p.showGraph = showGraph;
	}
}

void CTimeProfiler::PrintProfilingInfo() const
{
	LOG("%35s|%18s|%s", "Part", "Total Time", "Time of the last 0.5s");

	for (auto pi = profile.begin(); pi != profile.end(); ++pi) {
		const std::string& name = pi->first;
		const TimeRecord& tr = pi->second;

		LOG("%35s %16.2fms %5.2f%%", name.c_str(), tr.total.toMilliSecsf(), tr.percent * 100);
	}
}
