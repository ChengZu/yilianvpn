#ifndef LOOP_H
#define LOOP_H
#include <iostream>
#include <vector>
#include "Task.h"

class Loop
{
public:
	Loop(){
		this->id = 0;
		this->task = NULL;
		this->name = (char*)"default Loop";
		this->joined = false;
	}
	
	Loop(Task *task){
		this->id = task->getId();
		this->task = task;
		this->name = (char*)"default Loop";
		this->joined = false;
	}
	
	Loop(Task *task, char* name){
		this->id = task->getId();
		this->task = task;
		this->name = name;
		this->joined = false;
	}
	
	~Loop(){
		if(task != NULL){
			for(int i=0; i<tasks.size(); i++){
				Task* obj = dynamic_cast<Task*>(tasks[i]);
				if(*obj == *task){
					tasks.erase(tasks.begin() + i);
					i--;
					task == NULL;
					break;
				}
			}
		}

	}
	
	// 加入任务队列 
	bool join(){
		if(task == NULL || joined) return false;
		tasks.push_back(task);
		joined = true;
		return true;
	}
	
	void quit(){
		if(task != NULL)
			task->quit = true;
	}
	
	// 执行任务，移除完成任务 
	static void executeTasks(){
		for(int i=0; i<tasks.size(); i++){
			Task* obj = dynamic_cast<Task*>(tasks[i]);
			if(obj->quit){
				tasks.erase(tasks.begin() + i);
				i--;
				continue;
			}
			bool exit = obj->loop();
			if(exit){
				tasks.erase(tasks.begin() + i);
				i--;
			}
		}
	}
	
	// 返回任务容器大小 
	static int taskNum(){
		return tasks.size();
	}
	
private:
	// 所有任务容器 
	static std::vector<Task*> tasks;
	// 本loop任务 
	Task* task;
	// 任务id 
	long id;
	//任务名字 
	char* name;
	// 是否加入任务队列 
	bool joined; 
};

std::vector<Task*> Loop::tasks;

#endif
