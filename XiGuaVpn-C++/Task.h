#ifndef TASK_H
#define TASK_H

class Task
{
public:
    //是否退出
    bool quit;

    Task()
    {
        UID++;
        id = UID;
        quit = false;
    }

    virtual ~Task()
    {
        quit = true;
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

#endif

