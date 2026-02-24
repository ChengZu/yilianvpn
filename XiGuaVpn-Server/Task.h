#ifndef TASK_H
#define TASK_H
#include <iostream>
#include <vector>
using namespace std;

std::vector<void *> tasks;

class Task
{
public:
	//是否退出
	bool isQuit;

	Task()
	{
		UID++;
		id = UID;
		isQuit = false;
		create_task(this);
	}

	~Task()
	{
		for (int i = 0; i < tasks.size(); i++)
		{
			Task *task = (Task *)tasks[i];
			if(*task == *this)
			{
				tasks.erase(tasks.begin() + i);
				break;
			}
		}
	}

	void quit()
	{
		isQuit = true;
	}

	// 虚构函数loop
	virtual bool loop()
	{
		return false;
	}

	bool operator==(const Task &task)
	{
		return id == task.id;
	}

	long getId()
	{
		return id;
	}

	void create_task(Task *task)
	{
		tasks.push_back(task);
	}

private:
	// 任务id
	long id;
	static long UID;
};

long Task::UID = 0;

int task_loop()
{
	int ret = 0;
	for(int i = 0; i < tasks.size(); i++)
	{
		Task *task = (Task *)tasks[i];
		if(task->isQuit)
		{
			tasks.erase(tasks.begin() + i);
			i--;
			continue;
		}
		bool exit = task->loop();
		ret++;
		if(exit)
		{
			task->isQuit = true;
			tasks.erase(tasks.begin() + i);
			i--;
		}
	}
	return ret;
}

#endif

