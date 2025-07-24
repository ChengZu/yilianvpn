package com.vcvnc.xiguavpn;

public class RingQueueBuffer {
	// 头部，出队列方向 
	public int head;
	// 尾部，入队列方向
	public int tail;
	public int tag;
	// 队列总尺寸 
	public int capacity;
	// 队列空间 
	public byte[] space;
	public static final int EMPTY = 1;
	public static final int FULL = 2;
	
	RingQueueBuffer(){
		capacity = 65535;
		space = new byte[capacity];
		head = 0;
		tail = 0;
		tag = EMPTY;
	}
	
	RingQueueBuffer(int size){
		capacity = size;
		space = new byte[capacity];
		head = 0;
		tail = 0;
		tag = EMPTY;
	}
	
	public boolean isEmpty(){
		return (head == tail) && (tag == EMPTY);
	}
	
	public boolean isFull(){
		return (head == tail) && (tag == FULL);
	}
	
	public int availableReadLength() {
		if (isFull()) {
        	return capacity; // 如果队列为满，则返回 capacity
		} else {
			return (tail + capacity - head) % capacity; // 计算环形队列的可用长度
		}
	}
	
	public int availableWriteLength() {
		if (isEmpty()) {
        	return capacity; // 如果队列为空，则返回 capacity
		} else {
			return (head + capacity - tail) % capacity; // 计算环形队列的可用长度
		}
	}
	
	public int push(byte data){
		if(isFull()){
     		//System.out.printf("RingQueueBuffer is full.\n");
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
	
	public int push(byte[] data, int size){
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
	
	public short poll(){
		if(isEmpty()){
			//System.out.printf("RingQueueBuffer  is empty.\n");
			return -10000;
		}
   
		byte data = space[head];
   
   		head = (head + 1) % capacity;
   
		// 这个时候一定队列空了 
		if(tail == head){
			tag = EMPTY;
		}
		return data;
	}
	
	public byte[] poll(int size){
		byte[] data = new byte[size]; 
		for(int i = 0; i < size; i++){
			byte t = (byte) poll();
			if(t == -1){
				break;
			}
			data[i] = t;
		}
		return data;
	}
}
