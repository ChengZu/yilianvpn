#ifndef TASK_H
#define TASK_H
#include <iostream>
#include <vector>
using namespace std;

class Task
{
public:
    //是否退出
    bool isQuit;
    string name;

    Task()
    {
        UID++;
        id = UID;
        isQuit = false;
        name = "task";
    }

    virtual ~Task()
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
private:
    // 任务id
    long id;
    static long UID;
};

long Task::UID = 0;

std::vector<Task *> tasks;

void create_task(Task *task)
{	
	tasks.push_back(task);
}

void task_loop()
{
	for(int i = 0; i < tasks.size(); i++)
    {
        Task *task = dynamic_cast<Task *>(tasks[i]);
        if(task->isQuit)
        {
            tasks.erase(tasks.begin() + i);
            i--;
            continue;
        }
        bool exit = task->loop();
        if(exit)
        {
            task->isQuit = true;
            tasks.erase(tasks.begin() + i);
            i--;
        }
    }
}



#endif

