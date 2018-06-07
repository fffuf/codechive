#include <windows.h>
#include <malloc.h>    
#include <stdio.h>
#include <tchar.h>
#include <cstdint>
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <algorithm>

typedef BOOL (WINAPI *LPFN_GLPI)(
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, 
    PDWORD);

#define BYTE unsigned char

// Helper function to count set bits in the processor mask.
DWORD CountSetBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;    
    DWORD i;
    
    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
}

struct CacheInfo
{
	int line_size;
	int L1_size;
	int L2_size;
	int L3_size;
	CacheInfo()
	{
		LPFN_GLPI glpi;
		BOOL done = FALSE;
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
		DWORD returnLength = 0;
		DWORD logicalProcessorCount = 0;
		DWORD numaNodeCount = 0;
		DWORD processorCoreCount = 0;
		DWORD processorL1CacheCount = 0;
		DWORD processorL2CacheCount = 0;
		DWORD processorL3CacheCount = 0;
		DWORD processorPackageCount = 0;
		DWORD byteOffset = 0;
		PCACHE_DESCRIPTOR Cache;

		glpi = (LPFN_GLPI) GetProcAddress(
		                        GetModuleHandle(TEXT("kernel32")),
		                        "GetLogicalProcessorInformation");
		if (NULL == glpi) 
		{
		    _tprintf(TEXT("\nGetLogicalProcessorInformation is not supported.\n"));
		    return;
		}

		while (!done)
		{
		    DWORD rc = glpi(buffer, &returnLength);

		    if (FALSE == rc) 
		    {
		        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
		        {
		            if (buffer) 
		                free(buffer);

		            buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(
		                    returnLength);

		            if (NULL == buffer) 
		            {
		                _tprintf(TEXT("\nError: Allocation failure\n"));
		                return;
		            }
		        } 
		        else 
		        {
		            _tprintf(TEXT("\nError %d\n"), GetLastError());
		            return;
		        }
		    } 
		    else
		    {
		        done = TRUE;
		    }
		}

		ptr = buffer;

		while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) 
		{
			if (ptr->Relationship == RelationCache)
			{
		        // Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
		        Cache = &ptr->Cache;
				
		        if (Cache->Level == 1)
		        {
					line_size = Cache->LineSize;
					L1_size = Cache->Size;
		        }
		        else if (Cache->Level == 2)
		        {
		            L2_size = Cache->Size;
		        }
		        else if (Cache->Level == 3)
		        {
		            L3_size = Cache->Size;
		        }
		    }
		    byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		    ptr++;
		}
		free(buffer);
	}
};

class Action
{
public:
	LARGE_INTEGER operator() (BYTE* data, const int size);
};

LARGE_INTEGER Action::operator() (BYTE* data, const int size)
{
	LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency); 
	QueryPerformanceCounter(&StartingTime);

	for (int i = 1; i < size-1; ++i)
	{
		data[i] = (data[i-1] * data[i+1])%0xFF;
	}

	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	return ElapsedMicroseconds;
}

class Test
{
public:
	BYTE * data;
	int size;
	int num_threads;
	int num_action_runs;
	int num_test_runs;
	std::vector<long long> action_all_times;
	std::vector<long long> action_threadaverage_times;
	std::vector<long long> test_times;
	LARGE_INTEGER all_time;
	Test(const int _size,const int _num_threads, const int _num_action_runs, const int _num_test_runs)
		: size(_size)
		, num_threads(_num_threads)
		, num_action_runs(_num_action_runs)
		, num_test_runs(_num_test_runs)
		, data(nullptr)
	{}
	bool operator() ();
	void dump_time();
};

bool Test::operator() ()
{
	std::cout<<"Test with size="<<size<<" NThreads="<<num_threads<<" NARuns="<<num_action_runs<<" NTRuns="<<num_test_runs<<std::endl;
	data = new BYTE [size*(num_threads+1)];

	std::vector<BYTE*> data_parts(num_threads+1);
	BYTE * data_part = data;
	for (auto &it:data_parts)
	{
		it = data_part;
		data_part += size;
	}

	std::vector<std::future<LARGE_INTEGER> > threads_times(num_threads);

	Action action;

	LARGE_INTEGER  StartingTime, EndingTime, StartingTimeT, EndingTimeT, StartingTimeA, EndingTimeA, ElapsedMicroseconds;		
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency); 
	QueryPerformanceCounter(&StartingTime);																						/////////////////////////////////////////////

	for (int tn = 0; tn < num_test_runs; ++tn)
	{
		QueryPerformanceCounter(&StartingTimeT);																							/////////////////////////

		for (int ta = 0; ta < num_action_runs; ++ta)
		{
			QueryPerformanceCounter(&StartingTimeA);																								/////////
			for (int i = 0; i < num_threads; ++i)
			{
				threads_times[i] = std::async(std::launch::async,action,data_parts[i],size);
			}
			LARGE_INTEGER sum_time;
			sum_time.QuadPart = 0;
			for (auto & it:threads_times)
			{
				sum_time.QuadPart += (it.get()).QuadPart;
			}
			QueryPerformanceCounter(&EndingTimeA);																									/////////
			ElapsedMicroseconds.QuadPart = EndingTimeA.QuadPart - StartingTimeA.QuadPart;
			ElapsedMicroseconds.QuadPart *= 1000000;
			ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
			action_threadaverage_times.push_back(sum_time.QuadPart/num_threads);
			action_all_times.push_back(ElapsedMicroseconds.QuadPart);

			std::rotate(data_parts.begin(),data_parts.begin()+1,data_parts.end());
		}
		QueryPerformanceCounter(&EndingTimeT);																								/////////////////////////
		ElapsedMicroseconds.QuadPart = EndingTimeT.QuadPart - StartingTimeT.QuadPart;
		ElapsedMicroseconds.QuadPart *= 1000000;
		ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
		test_times.push_back(ElapsedMicroseconds.QuadPart);


	}


	QueryPerformanceCounter(&EndingTime);																						/////////////////////////////////////////////
	all_time.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;					
	all_time.QuadPart *= 1000000;
	all_time.QuadPart /= Frequency.QuadPart;

	delete[] data;
	data = nullptr;
	return true;
}

void Test::dump_time()
{
	long long sum_action_all = 0;
	long long sum_action_av = 0;
	long long sum_test = 0;
	std::cout<<"ALL TIME: "<<all_time.QuadPart<<std::endl;
	for (size_t i = 0; i < action_all_times.size(); ++i)
	{
		if (!(i % num_action_runs))
		{
			auto test_num = i/num_action_runs;
			sum_test += test_times[test_num];
			std::cout<<"/////////////////////// TEST# "<<test_num<<" ///////////////////////"<<std::endl;
			std::cout<<"TT: "<<test_times[test_num]<<std::endl;
			std::cout<<"------------------------------------------"<<std::endl;
		}
		//std::cout<<"==========================================="<<std::endl;
		//std::cout<<"AAV: "<<action_threadaverage_times[i]<<std::endl;
		//std::cout<<"AAll: "<<action_all_times[i]<<std::endl;
		//std::cout<<"------------------------------------------"<<std::endl;
		sum_action_all+=action_all_times[i];
		sum_action_av+=action_threadaverage_times[i];
	}
	std::cout<<"AVERAGE TEST: "<<sum_test/test_times.size()<<std::endl;
	std::cout<<"AVERAGE ACTION THREADAVERAGE: "<<sum_action_av/action_threadaverage_times.size()<<std::endl;
	std::cout<<"AVERAGE ACTION ALL: "<<sum_action_all/action_all_times.size()<<std::endl;
}


int main(int argc, char * argv[])
{

	CacheInfo cache;
	Test test1(cache.L3_size+1,1,1,10);
	test1();
	test1.dump_time();
	std::cout<<std::endl;
	Test test2((cache.L3_size+1)/2,2,1,10);
	test2();
	test2.dump_time();
	std::cout<<std::endl;
	Test test3(cache.L1_size,2,cache.L3_size/(2*cache.L1_size),10);
	test3();
	test3.dump_time();
	std::cout<<std::endl;
	Test test4(cache.L2_size,2,cache.L3_size/(2*cache.L2_size),10);
	test4();
	test4.dump_time();
	std::cout<<std::endl;
	Test test5(cache.line_size,2,cache.L3_size/(2*cache.line_size),10);
	test5();
	test5.dump_time();
	std::cout<<std::endl;
	Test test6(cache.L1_size,4,cache.L3_size/(4*cache.L1_size),10);
	test6();
	test6.dump_time();
	std::cout<<std::endl;
	Test test7(cache.L2_size,4,cache.L3_size/(4*cache.L2_size),10);
	test7();
	test7.dump_time();
	std::cout<<std::endl;
	Test test8(cache.L3_size,4,1,10);
	test8();
	test8.dump_time();
	std::cout<<std::endl;
	return 0;
}