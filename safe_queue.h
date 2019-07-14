#include <limits>
#include <deque>
#include <iostream>
#include <unistd.h>
#include <mutex>
#include <condition_variable>

template <class T> class SafeQueue{ 
    private:
        std::mutex d_mutex;
        std::condition_variable p_condition;        //producer 
        std::condition_variable c_condition;        //consumer
        std::deque<T> queue;  //initialize a local queue
        int max_size;

    public:
        SafeQueue(int my_size) : max_size(my_size) {};
        SafeQueue() : max_size(std::numeric_limits<unsigned int>::max()) {};


        void safe_push(T const& item){  //max size
		{
			std::unique_lock<std::mutex> lock(this->d_mutex); 
			this->queue.push_front(item);
		}
            this->c_condition.notify_one();       //wake a consumer since 1 element has been pushed in
        }

        bool safe_push_try(T item){ 
            if(d_mutex.try_lock()){
                this->queue.push(item);
                c_condition.notify_one();
                return true;
            }
            return false;
        }

        int safe_size(){
             std::lock_guard<std::mutex> lock(d_mutex);
             return this->queue.size();
        }

        T safe_pop(){
            std::unique_lock<std::mutex> lock(d_mutex); 
            this->c_condition.wait(lock, [=]{return !this->queue.empty();});   //if holds, it goes through
            T popped(std::move(this->queue.back()));
            this->queue.pop_back();
            return popped;
        }

        void empty_and_print(){
            while(!this->queue.empty()){
                T popped = this->queue.front();
                this->queue.pop();
                std::cout << popped << std::endl;
            }
        }

        bool isEmpty(){
            std::unique_lock<std::mutex> lock(d_mutex);
            return this->queue.empty();
        }

};
