#ifndef RINGQUEUEBUFFER_H
#define RINGQUEUEBUFFER_H

class RingQueueBuffer
{
public:
	// 头部，出队列方向 
	unsigned int head;
	// 尾部，入队列方向
	unsigned int tail;
	unsigned int tag;
	// 队列总尺寸 
	unsigned int capacity;
	// 队列空间 
	char* space;
	static const int EMPTY = 1;
	static const int FULL = 2;
	
	RingQueueBuffer(){
		capacity = 65535;
		space = new char[capacity];
		head = 0;
		tail = 0;
		tag = EMPTY;
	}
	
	RingQueueBuffer(int size){
		capacity = size;
		space = new char[capacity];
		head = 0;
		tail = 0;
		tag = EMPTY;
	}
	
	~RingQueueBuffer(){
		delete[] space;	
	}
	
	bool isEmpty(){
		return (head == tail) && (tag == EMPTY);
	}
	
	bool isFull(){
		return (head == tail) && (tag == FULL);
	}
	
	int availableReadLength() {
		if (isFull()) {
        	return capacity; // 如果队列为满，则返回 capacity
		} else {
			return (tail + capacity - head) % capacity; // 计算环形队列的可用长度
		}
	}
	
	int availableWriteLength() {
		if (isEmpty()) {
        	return capacity; // 如果队列为空，则返回 capacity
		} else {
			return (head + capacity - tail) % capacity; // 计算环形队列的可用长度
		}
	}
	
	int push(char data){
		if(isFull()){
     		// printf("RingQueueBuffer is full.\n"); 
			return -1;
		}

   		space[tail] = data;
		tail = (tail + 1) % capacity;
   
		// 这个时候一定队列满了 
		if(tail == head){
			tag = FULL;
		}
		return tag ; 
	}
	
	int push(char* data, int size){
		int ret = size;
		for(int i = 0; i < size; i++){
			int res = push(data[i]);
			if(res == -1){
				ret = i;
				break;
			}
		}
		return ret;
	}
	
	int poll(char* data){
		if(isEmpty()){
			// printf("RingQueueBuffer  is empty.\n"); 
			return -1;
		}
   
		*data = space[head];
   
   		head = (head + 1) % capacity;
   
		// 这个时候一定队列空了 
		if(tail == head){
			tag = EMPTY;
		}
		return tag;
	}
	
	int poll(char* data, int size){
		int ret = size;
		for(int i = 0; i < size; i++){
			int res = poll(&data[i]);
			if(res == -1){
				ret = i;
				break;
			}
		}
		return ret;
	}
	
protected:
};

#endif
