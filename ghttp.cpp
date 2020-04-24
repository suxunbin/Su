#define THREAD_NUM 30

class Task
{
public:
	bool update;
	string db_name;
	string format;
	string db_query;
	const shared_ptr<HttpServer::Response> response;
	const shared_ptr<HttpServer::Request> request;
	Task(bool flag, string name, string ft, string query, const shared_ptr<HttpServer::Response>& res, const shared_ptr<HttpServer::Request>& req);
	~Task();
	void run();
};
Task::Task(bool flag, string name, string ft, string query, const shared_ptr<HttpServer::Response>& res, const shared_ptr<HttpServer::Request>& req):response(res),request(req)
{
	update = flag;
	db_name = name;
	format = ft;
	db_query = query;
}
Task::~Task()
{

}
void Task::run()
{
	query_thread(update, db_name, format, db_query, response, request);
}

class Thread
{
public:
	thread TD;
	int ID;
	static int threadnum;
	Task* task;
	Thread();
	~Thread();
	int GetThreadID();
	void assign(Task* t);
	void run();
	void start();
	friend bool operator==(Thread t1, Thread t2);
	friend bool operator!=(Thread t1, Thread t2);
};

list<Thread*> busythreads;
vector<Thread*> freethreads;
mutex busy_mutex;
mutex free_mutex;
mutex task_mutex;
condition_variable task_cond;

void BackToFree(Thread *t)
{
	busy_mutex.lock();
	busythreads.erase(find(busythreads.begin(), busythreads.end(), t));
	busy_mutex.unlock();

	free_mutex.lock();
	freethreads.push_back(t);
	free_mutex.unlock();
}

int Thread::threadnum = 0;

Thread::Thread()
{
	threadnum++;
	ID = threadnum;
}
Thread::~Thread()
{

}
int Thread::GetThreadID()
{
	return ID;
}
void Thread::assign(Task* t)
{
	task = t;
}
void Thread::run()
{
	cout << "Thread:" << ID << " run\n";
	task->run();
	delete task;
	BackToFree(this);
}
void Thread::start()
{
	TD = thread(&Thread::run, this);
	TD.detach();
}
bool operator==(Thread t1, Thread t2)
{
	return t1.ID == t2.ID;
}
bool operator!=(Thread t1, Thread t2)
{
	return !(t1.ID == t2.ID);
}

class ThreadPool
{
public:
	int ThreadNum;
	bool isclose;
	thread ThreadsManage;
	queue<Task*> tasklines;
	ThreadPool();
	ThreadPool(int t);
	~ThreadPool();
	void create();
	void SetThreadNum(int t);
	int GetThreadNum();
	void AddTask(Task* t);
	void start();
	void close();
};
ThreadPool::ThreadPool()
{
	isclose = false;
	ThreadNum = 10;
	busythreads.clear();
	freethreads.clear();
	for (int i = 0; i < ThreadNum; i++)
	{
		Thread *p = new Thread();
		freethreads.push_back(p);
	}
}
ThreadPool::ThreadPool(int t)
{
	isclose = false;
	ThreadNum = t;
	busythreads.clear();
	freethreads.clear();
	for (int i = 0; i < t; i++)
	{
		Thread *p = new Thread();
		freethreads.push_back(p);
	}
}
ThreadPool::~ThreadPool()
{
	for (vector<Thread*>::iterator i = freethreads.begin(); i != freethreads.end(); i++)
		delete *i;
}
void ThreadPool::create()
{
	ThreadsManage = thread(&ThreadPool::start, this);
	ThreadsManage.detach();
}
void ThreadPool::SetThreadNum(int t)
{
	ThreadNum = t;
}
int ThreadPool::GetThreadNum()
{
	return ThreadNum;
}
void ThreadPool::AddTask(Task* t)
{
	unique_lock<mutex> locker(task_mutex);
	tasklines.push(t);
	locker.unlock();
	task_cond.notify_one();
}
void ThreadPool::start()
{
	while (true)
	{
		if (isclose == true)
		{
			busy_mutex.lock();
			if (busythreads.size() != 0)
			{
				busy_mutex.unlock();
				continue;
			}
			busy_mutex.unlock();
			break;
		}

		free_mutex.lock();
		if (freethreads.size() == 0)
		{
			free_mutex.unlock();
			continue;
		}
		free_mutex.unlock();

		unique_lock<mutex> locker(task_mutex);
		while (tasklines.size() == 0)
			task_cond.wait(locker);

		Task *job = tasklines.front();
		tasklines.pop();
		locker.unlock();

		free_mutex.lock();
		Thread *t = freethreads.back();
		freethreads.pop_back();
		t->assign(job);
		free_mutex.unlock();

		busy_mutex.lock();
		busythreads.push_back(t);
		busy_mutex.unlock();

		t->start();
	}
}
void ThreadPool::close()
{
	isclose = true;
}

ThreadPool pool(THREAD_NUM);

pool.create();
