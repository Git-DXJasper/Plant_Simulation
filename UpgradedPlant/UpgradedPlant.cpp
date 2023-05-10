//HW3_Xie_Dongze
/*
* Cost
per worker 200
per unit capacity(20,20,30,30,40)
per new buffer 100
* Profit
per product 100

*/
#include <iostream>
#include <mutex>
#include <fstream>
#include <thread>
#include <vector>
#include <condition_variable>
#include <string>
#include <atomic>

using namespace std;
using namespace chrono;
using namespace literals::chrono_literals;

mutex m_print, m_buf;
condition_variable cv1, cv2;
ofstream fout{ ofstream("log.txt") };
const vector<int> make_part{ 500,500,600,600,700 }, move_part{ 200,200,300,300,400 }, assembly_part{ 600,600,700,700,800 };
const int MaxTimePart{ 30000 }, MaxTimeProduct{ 28000 };

vector<string> status_part{ "New Load Order","Wake up-Notified","Wake up-Timeout" };
vector<string> status_product{ "New Pickup Order","Wake up-Notified","Wake up-Timeout" };

class Buffer {
public:
	const vector<int> buf_capacity{ 13,13,13,13,13 };
	vector<vector<int>> buf_state;

	Buffer() {
		buf_state = { {0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0} };
		return;
	}
};

class PartWorker {
private:
	int id;
public:
	vector<int> load_order;
	int getId() { return id; }
	PartWorker(int id) {//constructor
		this->id = id;
		this->load_order = { 0,0,0,0,0 };
	}
	void loadParts(int id, Buffer& buffer, int& sum, int iteration, int status, system_clock::duration acc_wait_time, atomic<int>& p);
	void doOneLoadOrder(int id, Buffer& buffer, int iteration, atomic<int>& p);
};

class ProductWorker {
private:
	int id;
public:
	vector<int> pickup_order_real;
	vector<int> cart_state;
	vector<int> local_state;
	int getId() { return id; }
	ProductWorker(int id) {//constructor
		this->id = id;
		this->pickup_order_real = { 0,0,0,0,0 };
		this->cart_state = { 0,0,0,0,0 };
		this->local_state = { 0,0,0,0,0 };
	}
	void pickupParts(int id, Buffer& buffer, int& sum, system_clock::time_point& start_pickup_time,
		vector<vector<int>>& prev_buf_state, vector<int>& prev_pickup_order, vector<int>& prev_local_state, vector<int>& prev_cart_state,
		vector<vector<int>>& updated_buf_state, vector<int>& updated_pickup_order, vector<int>& updated_local_state, vector<int>& updated_cart_state, atomic<int>& c);
	void fulfill(int id, system_clock::time_point& end_assembly_time,
		vector<int>& final_local_state, vector<int>& final_cart_state);
	void partialFulfill(int id, system_clock::time_point& end_assembly_time,
		vector<int>& final_local_state, vector<int>& final_cart_state);
	void doOnePickupOrder(int id, Buffer& buffer, int iteration, atomic<int>& c, atomic<int>& total_complete);


};

ostream& operator<<(ostream& str, vector<int> v);
ostream& operator<<(ostream& str, vector<vector<int>> v);
vector<int> randLoadOrder(vector<int> cur);
vector<int> randPickUpOrder(vector<int> cur);
bool isLegalPO(vector<int> cur, vector<int> temp);
void doParts(PartWorker& worker, Buffer& buffer, int num_iter, atomic<int>& p);
void doProducts(ProductWorker& worker, Buffer& buffer, int num_iter, atomic<int>& c, atomic<int>& total_complete);
void printPartWLog(PartWorker& worker, int iteration, system_clock::time_point start_load_time, int status,
	system_clock::duration acc_wait_time, vector<vector<int>> prev_buf_state, vector<int> prev_load_order,
	vector<vector<int>> updated_buf_state, vector<int> updated_load_order);

void printProductWLog_temp(ProductWorker& worker, int iteration, system_clock::time_point start_pickup_time, int status,
	system_clock::duration acc_wait_time, vector<vector<int>> prev_buf_state, vector<int> prev_pickup_order,
	vector<int> prev_local_state, vector<int> prev_cart_state, vector<vector<int>> updated_buf_state,
	vector<int> updated_pickup_order, vector<int> updated_local_state, vector<int> updated_cart_state,
	atomic<int>& total_complete);

void printProductWLog_final(ProductWorker& worker, int iteration, system_clock::time_point start_pickup_time, int status,
	system_clock::duration acc_wait_time, vector<vector<int>> prev_buf_state, vector<int> prev_pickup_order,
	vector<int> prev_local_state, vector<int> prev_cart_state, vector<vector<int>> updated_buf_state,
	vector<int> updated_pickup_order, vector<int> updated_local_state, vector<int> updated_cart_state,
	system_clock::time_point end_assembly_time, vector<int> final_local_state, vector<int> final_cart_state,
	atomic<int>& total_complete);
void preFill(PartWorker& worker, Buffer& buffer);


int main() {
	const int m{ 50 }, n{ 60 };
	atomic<int> total_complete{ 0 };
	atomic<int> p{ 1 }; //producer pointer
	atomic<int> c{ 0 }; //consumer pointer
	Buffer buffer;

	vector<PartWorker> part_workers;
	vector<ProductWorker> product_workers;
	

	for (int i = 0; i <= m; i++)
		part_workers.emplace_back(PartWorker(i + 1));
	for (int i = 0; i <= n; i++)
		product_workers.emplace_back(ProductWorker(i + 1));

	auto time_luanch = system_clock::now();

	vector<thread> Tprepare;
	for (int i = 0; i < buffer.buf_capacity[0];i++) {
		Tprepare.emplace_back(preFill, ref(part_workers[i]), ref(buffer));
	}
	for (auto& i : Tprepare) i.join();

	vector<thread> PartW, ProductW;

	for (int i = 0; i < m; ++i) {
		PartW.emplace_back(doParts, ref(part_workers[i]), ref(buffer), 20, ref(p));
	}
	for (int i = 0; i < n; ++i) {
		ProductW.emplace_back(doProducts, ref(product_workers[i]), ref(buffer), 20, ref(c), ref(total_complete));
	}
	for (auto& i : PartW) i.join();
	for (auto& i : ProductW) i.join();
	fout << "Finish!" << endl;
	auto time_complete = system_clock::now();

	cout << "Number of prodcuts completed: " << total_complete << endl;
	cout << "Worker cost: " << 22000 << endl;
	int mc{ (int)(buffer.buf_state.size() * (100 + 140 * buffer.buf_capacity[0])) };

	cout << "Machine cost: " << mc  << endl;
	cout << "Overall profit: " << total_complete * 100 - mc - 20000 << endl;
	cout << "Time cost: " << duration_cast<milliseconds>(time_complete - time_luanch).count() << "ms" << endl;

	return 0;
}

void PartWorker::loadParts(int id, Buffer& buffer, int& sum, int iteration, int status, system_clock::duration acc_wait_time, atomic<int>& p) {
	int orgin_sum{ sum };
	system_clock::time_point start_load_time{ system_clock::now() };
	vector<vector<int>> prev_buf_state, updated_buf_state;
	vector<int> prev_load_order, updated_load_order;
	prev_buf_state = buffer.buf_state;
	prev_load_order = this->load_order;

	for (int i = 0;i < 5;i++) {//load parts onto available slots
		int max_available{ 0 };
		if (buffer.buf_capacity[i] - buffer.buf_state[p][i] >= this->load_order[i])
			max_available = this->load_order[i];
		else
			max_available = buffer.buf_capacity[i] - buffer.buf_state[p][i];

		buffer.buf_state[p][i] += max_available;
		this->load_order[i] -= max_available;
	}
	updated_buf_state = buffer.buf_state;
	updated_load_order = this->load_order;
	//reset and update sum
	sum = 0;
	for (auto i : this->load_order) sum += i;
	if (status == 0) {//if this is the new order's first attempt
		if (sum == 0) {//completed, using the current buffer
			printPartWLog(*this, iteration, start_load_time, status, acc_wait_time, prev_buf_state, prev_load_order, updated_buf_state, updated_load_order);
			return;
		}
		else {//not completed, try next buffer
			//update index pointer p
			int expected{ p.load(memory_order_seq_cst) };
			int desired{ (expected + 1) % (int)buffer.buf_state.size()};
			while (!p.compare_exchange_weak(expected, desired, std::memory_order_seq_cst)) {
				desired = (expected + 1) % (int)buffer.buf_state.size();
			}
			//p turns into the next buffer for partworker, atomically
			//try to load again
			for (int i = 0;i < 5;i++) {//load parts onto available slots
				int max_available{ 0 };
				if (buffer.buf_capacity[i] - buffer.buf_state[p][i] >= this->load_order[i])
					max_available = this->load_order[i];
				else
					max_available = buffer.buf_capacity[i] - buffer.buf_state[p][i];

				buffer.buf_state[p][i] += max_available;
				this->load_order[i] -= max_available;
			}
			updated_buf_state = buffer.buf_state;
			updated_load_order = this->load_order;
			//reset and update sum
			sum = 0;
			for (auto i : this->load_order) sum += i;
		}
	}
	if (orgin_sum != sum) //only print if do something to buffer, if wake up and find nothing can do and go back wait, then don't print
		printPartWLog(*this, iteration, start_load_time, status, acc_wait_time, prev_buf_state, prev_load_order, updated_buf_state, updated_load_order);

	return;
}

void PartWorker::doOneLoadOrder(int id, Buffer& buffer, int iteration, atomic<int>& p) {
	int sleeptime{ 0 }, sum{ 0 }, status{ 0 };
	system_clock::duration acc_wait_time{ 0 };


	//start from generating new load order
	vector<int> new_loadOrder{ randLoadOrder(this->load_order) };

	for (int i = 0;i < 5;i++) {//caclulate time for make and move all parts to the buffer area
		sleeptime += (new_loadOrder[i] - this->load_order[i]) * (make_part[i] + move_part[i]);
	}
	this_thread::sleep_for(microseconds(sleeptime));

	this->load_order = new_loadOrder;

	//make and move complete, now load parts onto the buffer

	unique_lock<mutex> Ulock1(m_buf);

	loadParts(id, buffer, sum, iteration, 0, acc_wait_time, p);

	if (sum == 0) {//current load order complete,and nothing left to move back
		cv2.notify_all();
		return;
	}
	//sum != 0 , so there is still parts left
	//wait near buffer area and keep trying to load parts until timeout
	auto wait_begin{ system_clock::now() };

	while (sum != 0 && cv1.wait_until(Ulock1, wait_begin + microseconds(MaxTimePart)) != cv_status::timeout) {
		//if being notified and not timeout yet, then try to load agian
		acc_wait_time = system_clock::now() - wait_begin;
		loadParts(id, buffer, sum, iteration, 1, acc_wait_time, p);
	}
	cv2.notify_all();//done operations with buffer
	//break the while loop since sum=0 and order complete
	if (sum == 0) return;

	//break the while loop since timeout, need to move current remaing load order back
	auto timeout_wake_time{ system_clock::now() };
	acc_wait_time = timeout_wake_time - wait_begin;

	//print after timeout, status = 2(timeout)
	printPartWLog(*this, iteration, timeout_wake_time, 2, acc_wait_time, buffer.buf_state, this->load_order, buffer.buf_state, this->load_order);
	sleeptime = 0;//reset sleeptime
	for (int i = 0;i < 5;i++) {//caclulate time for moving back
		sleeptime += this->load_order[i] * move_part[i];
	}
	this_thread::sleep_for(microseconds(sleeptime));

	//one load order process complete 

	return;

}

void ProductWorker::pickupParts(int id, Buffer& buffer, int& sum, system_clock::time_point& start_pickup_time,
	vector<vector<int>>& prev_buf_state, vector<int>& prev_pickup_order, vector<int>& prev_local_state, vector<int>& prev_cart_state,
	vector<vector<int>>& updated_buf_state, vector<int>& updated_pickup_order, vector<int>& updated_local_state, vector<int>& updated_cart_state, atomic<int>& c) {

	start_pickup_time = system_clock::now();
	prev_buf_state = buffer.buf_state;
	prev_pickup_order = this->pickup_order_real;
	prev_local_state = this->local_state;
	prev_cart_state = this->cart_state;

	for (int i = 0;i < 5;i++) {//pick up parts from buffer to the cart
		int max_available{ 0 };
		if (buffer.buf_state[c][i] >= this->pickup_order_real[i]) {
			max_available = this->pickup_order_real[i];
		}
		else {
			max_available = buffer.buf_state[c][i];
		}

		buffer.buf_state[c][i] -= max_available;
		this->pickup_order_real[i] -= max_available;
		this->cart_state[i] += max_available;
	}

	updated_buf_state = buffer.buf_state;
	updated_pickup_order = this->pickup_order_real;
	updated_local_state = this->local_state;
	updated_cart_state = this->cart_state;

	sum = 0;
	for (auto i : this->pickup_order_real)sum += i;
	return;
}

void ProductWorker::fulfill(int id, system_clock::time_point& end_assembly_time,
	vector<int>& final_local_state, vector<int>& final_cart_state) {
	int sleeptime{ 0 };
	for (int i = 0;i < 5;i++) {
		sleeptime += this->cart_state[i] * move_part[i];
		this->local_state[i] += this->cart_state[i];
		this->cart_state[i] = 0;
	}
	this_thread::sleep_for(microseconds(sleeptime));
	//move complete, now assembly
	sleeptime = 0;
	for (int i = 0;i < 5;i++) {
		sleeptime += this->local_state[i] * assembly_part[i];
		this->local_state[i] = 0;
	}
	this_thread::sleep_for(microseconds(sleeptime));
	end_assembly_time = system_clock::now();
	final_local_state = this->local_state;
	final_cart_state = this->cart_state;

	//one pickup order process fulfilled, with empty local area and cart

	return;
}

void ProductWorker::partialFulfill(int id, system_clock::time_point& end_assembly_time,
	vector<int>& final_local_state, vector<int>& final_cart_state) {

	int sleeptime{ 0 };
	for (int i = 0;i < 5;i++) {
		//move parts in cart to local area, merge with parts already in local area
		sleeptime += this->cart_state[i] * move_part[i];
		this->local_state[i] += cart_state[i];
		//reset cart
		this->cart_state[i] = 0;
	}
	this_thread::sleep_for(microseconds(sleeptime));
	//can't aseembly since partial fulfilled, take the time after merge as end time
	end_assembly_time = system_clock::now();

	//current pickup order partial fulfilled, local state indicates remaining parts
	final_local_state = this->local_state;
	final_cart_state = this->cart_state;

	return;
}

void ProductWorker::doOnePickupOrder(int id, Buffer& buffer, int iteration, atomic<int>& c, atomic<int>& total_complete) {
	int sleeptime{ 0 }, sum{ 0 }, status{ 0 };
	system_clock::time_point start_pickup_time, end_assembly_time;
	system_clock::duration acc_wait_time{ 0 };
	vector<vector<int>> prev_buf_state, updated_buf_state;
	vector<int> prev_pickup_order, prev_local_state, prev_cart_state,
		updated_pickup_order, updated_local_state, updated_cart_state,
		final_local_state, final_cart_state;

	//start from generating new pickup order
	vector<int> new_pickupOrder{ randLoadOrder(this->local_state) };
	for (int i = 0;i < 5;i++) {
		this->pickup_order_real[i] = new_pickupOrder[i] - this->local_state[i];
	}

	//product worker pick up parts from the buffer at buffer area

	unique_lock<mutex> Ulock2(m_buf);

	pickupParts(id, buffer, sum, start_pickup_time, prev_buf_state, prev_pickup_order, prev_local_state, prev_cart_state,
		updated_buf_state, updated_pickup_order, updated_local_state, updated_cart_state, c);


	if (sum == 0) {//real pick up order decreased to empty, already pickup all parts needed
		cv1.notify_all();
		fulfill(id, end_assembly_time, final_local_state, final_cart_state);
		++total_complete;
		//print if fulfill order in one shot, status = 0(new order)
		printProductWLog_final(*this, iteration, start_pickup_time, 0, acc_wait_time,
			prev_buf_state, prev_pickup_order, prev_local_state, prev_cart_state,
			updated_buf_state, updated_pickup_order, updated_local_state, updated_cart_state,
			end_assembly_time, final_local_state, final_cart_state, total_complete);

		return;
	}
	
	//update index pointer c
	int expected{ c.load(memory_order_seq_cst) };
	int desired{ (expected + 1) % (int)buffer.buf_state.size() };
	while (!c.compare_exchange_weak(expected, desired, std::memory_order_seq_cst)) {
		desired = (expected + 1) % (int)buffer.buf_state.size();
	}
	//c turns into the next buffer for partworker, atomically
	//try to pick up agian
	for (int i = 0;i < 5;i++) {//pick up parts from buffer to the cart
		int max_available{ 0 };
		if (buffer.buf_state[c][i] >= this->pickup_order_real[i]) {
			max_available = this->pickup_order_real[i];
		}
		else {
			max_available = buffer.buf_state[c][i];
		}

		buffer.buf_state[c][i] -= max_available;
		this->pickup_order_real[i] -= max_available;
		this->cart_state[i] += max_available;
	}

	updated_buf_state = buffer.buf_state;
	updated_pickup_order = this->pickup_order_real;
	updated_local_state = this->local_state;
	updated_cart_state = this->cart_state;
	//reset and update sum
	sum = 0;
	for (auto i : this->pickup_order_real)sum += i;
	if (sum == 0) {//pick up all needed with the second attempt
		cv1.notify_all();
		fulfill(id, end_assembly_time, final_local_state, final_cart_state);
		++total_complete;
		//print if fulfill order in one shot, status = 0(new order)
		printProductWLog_final(*this, iteration, start_pickup_time, 0, acc_wait_time,
			prev_buf_state, prev_pickup_order, prev_local_state, prev_cart_state,
			updated_buf_state, updated_pickup_order, updated_local_state, updated_cart_state,
			end_assembly_time, final_local_state, final_cart_state, total_complete);

		return;
	}

	//print before wait, afer first attempt of pickup, status = 0(new order)
	printProductWLog_temp(*this, iteration, start_pickup_time, 0, acc_wait_time,
		prev_buf_state, prev_pickup_order, prev_local_state, prev_cart_state,
		updated_buf_state, updated_pickup_order, updated_local_state, updated_cart_state, total_complete);
	//wait near buffer area, keep trying to pickup parts until timeout
	auto wait_begin = system_clock::now();

	while (sum != 0 && cv2.wait_until(Ulock2, wait_begin + microseconds(MaxTimePart)) != cv_status::timeout) {
		acc_wait_time = system_clock::now() - wait_begin;
		int origin_sum{ sum };
		//if being notified and not timeout yet, then try to pickup agian
		pickupParts(id, buffer, sum, start_pickup_time,
			prev_buf_state, prev_pickup_order, prev_local_state, prev_cart_state,
			updated_buf_state, updated_pickup_order, updated_local_state, updated_cart_state, c);

		//print once after being notified, status = 1(wakeup notified)
		if (origin_sum != sum)
			printProductWLog_temp(*this, iteration, start_pickup_time, 1, acc_wait_time,
				prev_buf_state, prev_pickup_order, prev_local_state, prev_cart_state,
				updated_buf_state, updated_pickup_order, updated_local_state, updated_cart_state, total_complete);
	}
	cv1.notify_all();//done operations with buffer
	//break the while loop since sum=0 and order fulfilled
	if (sum == 0) {
		fulfill(id, end_assembly_time, final_local_state, final_cart_state);
		++total_complete;

		//print if wake up due to order complete, status = 1(wakeup notified)
		printProductWLog_final(*this, iteration, start_pickup_time, 1, acc_wait_time,
			prev_buf_state, prev_pickup_order, prev_local_state, prev_cart_state,
			updated_buf_state, updated_pickup_order, updated_local_state, updated_cart_state,
			end_assembly_time, final_local_state, final_cart_state, total_complete);

		return;
	}
	//break the while loop since timeout, order partial fulfilled
	acc_wait_time = system_clock::now() - wait_begin;
	partialFulfill(id, end_assembly_time, final_local_state, final_cart_state);

	//print if after timeout still can't complete, order partial fulfilled, status = 2(wakeup timeout)
	printProductWLog_final(*this, iteration, start_pickup_time, 2, acc_wait_time,
		prev_buf_state, prev_pickup_order, prev_local_state, prev_cart_state,
		updated_buf_state, updated_pickup_order, updated_local_state, updated_cart_state,
		end_assembly_time, final_local_state, final_cart_state, total_complete);

	return;

}

void doParts(PartWorker& worker, Buffer& buffer, int num_iter, atomic<int>& p) {
	//finish 5 iterations
	for (int i = 0;i < num_iter;i++) {
		worker.doOneLoadOrder(worker.getId(), buffer, i + 1, p);
	}
	return;
}

void doProducts(ProductWorker& worker, Buffer& buffer, int num_iter, atomic<int>& c, atomic<int>& total_complete) {
	//finish 5 iterations
	for (int i = 0;i < num_iter;i++) {
		worker.doOnePickupOrder(worker.getId(), buffer, i + 1, c, total_complete);
	}
	return;
}

vector<int> randLoadOrder(vector<int> cur) {
	srand(system_clock::now().time_since_epoch().count());
	int sum{ 0 };
	for (auto i : cur)sum += i;
	for (int i = 0;i < 6 - sum;i++) cur[rand() % 5]++;
	return cur;
}

vector<int> randPickUpOrder(vector<int> cur) {
	srand(system_clock::now().time_since_epoch().count());
	int sum{ 0 };
	for (auto i : cur) sum += i;

	vector<int> temp;

	do {
		temp = cur;
		for (int i = 0;i < 5 - sum;i++) temp[rand() % 5]++;
	} while (!isLegalPO(cur, temp));

	cur = temp;
	return cur;
}

bool isLegalPO(vector<int> cur, vector<int> temp) {
	int sum{ 0 }, zeros{ 0 };
	for (int i = 0;i < 5;i++) {
		if (temp[i] > 5) return false; //illegal
		if (temp[i] - cur[i] < 0) return false; //not reuse
		if (temp[i] == 0) zeros++;
		sum += temp[i];
	}
	if (sum == 5 && (zeros == 2 || zeros == 3)) return true;
	else return false;
}

ostream& operator<<(ostream& str, vector<int> v) {
	str << "(";
	for (int i = 0;i < v.size() - 1;i++) {
		str << v[i] << ",";
	}
	str << v[v.size() - 1] << ")";
	return str;
}

ostream& operator<<(ostream& str, vector<vector<int>> v) {
	str << "[";
	for (auto i : v) {
		str << i << " ";
	}
	str << "]";
}

void printPartWLog(PartWorker& worker, int iteration, system_clock::time_point start_load_time, int status,
	system_clock::duration acc_wait_time, vector<vector<int>> prev_buf_state, vector<int> prev_load_order,
	vector<vector<int>> updated_buf_state, vector<int> updated_load_order) {

	lock_guard<mutex> LG(m_print);

	fout << "Current Time: " << start_load_time.time_since_epoch().count() << " us" << endl;
	fout << "Iteration " << iteration << endl;
	fout << "Part Worker ID: " << worker.getId() << endl;
	fout << "Status: " << status_part[status] << endl;
	fout << "Accumulate Wait Time: " << duration_cast<microseconds>(acc_wait_time).count() << " us" << endl;
	fout << "Buffer State: " << prev_buf_state << endl;
	fout << "Load Order: " << prev_load_order << endl;
	fout << "Updated Buffer State: " << updated_buf_state << endl;
	fout << "Updated Load Order: " << updated_load_order << endl;
	fout << endl;

	return;
}

void printProductWLog_temp(ProductWorker& worker, int iteration, system_clock::time_point start_pickup_time, int status,
	system_clock::duration acc_wait_time, vector<vector<int>> prev_buf_state, vector<int> prev_pickup_order,
	vector<int> prev_local_state, vector<int> prev_cart_state, vector<vector<int>> updated_buf_state,
	vector<int> updated_pickup_order, vector<int> updated_local_state, vector<int> updated_cart_state,
	atomic<int>& total_complete) {

	lock_guard<mutex> LG(m_print);

	fout << "Current Time: " << start_pickup_time.time_since_epoch().count() << " us" << endl;
	fout << "Iteration " << iteration << endl;
	fout << "Product Worker ID:" << worker.getId() << endl;
	fout << "Status: " << status_product[status] << endl;
	fout << "Accumulate Wait Time: " << duration_cast<microseconds>(acc_wait_time).count() << " us" << endl;
	fout << "Buffer State: " << prev_buf_state << endl;
	fout << "Pickup Order: " << prev_pickup_order << endl;
	fout << "Local State: " << prev_local_state << endl;
	fout << "Cart State: " << prev_cart_state << endl;
	fout << "Updated Buffer State: " << updated_buf_state << endl;
	fout << "Updated Pickup Order: " << updated_pickup_order << endl;
	fout << "Updated Local State: " << updated_local_state << endl;
	fout << "Updated Cart State: " << updated_cart_state << endl;
	fout << "Total Completed Products: " << total_complete << endl;
	fout << endl;

	return;
}

void printProductWLog_final(ProductWorker& worker, int iteration, system_clock::time_point start_pickup_time, int status,
	system_clock::duration acc_wait_time, vector<vector<int>> prev_buf_state, vector<int> prev_pickup_order,
	vector<int> prev_local_state, vector<int> prev_cart_state, vector<vector<int>> updated_buf_state,
	vector<int> updated_pickup_order, vector<int> updated_local_state, vector<int> updated_cart_state,
	system_clock::time_point end_assembly_time, vector<int> final_local_state, vector<int> final_cart_state,
	atomic<int>& total_complete) {

	lock_guard<mutex> LG(m_print);

	fout << "Current Time: " << start_pickup_time.time_since_epoch().count() << " us" << endl;
	fout << "Iteration " << iteration << endl;
	fout << "Product Worker ID:" << worker.getId() << endl;
	fout << "Status: " << status_product[status] << endl;
	fout << "Accumulate Wait Time: " << duration_cast<microseconds>(acc_wait_time).count() << " us" << endl;
	fout << "Buffer State: " << prev_buf_state << endl;
	fout << "Pickup Order: " << prev_pickup_order << endl;
	fout << "Local State: " << prev_local_state << endl;
	fout << "Cart State: " << prev_cart_state << endl;
	fout << "Updated Buffer State: " << updated_buf_state << endl;
	fout << "Updated Pickup Order: " << updated_pickup_order << endl;
	fout << "Updated Local State: " << updated_local_state << endl;
	fout << "Updated Cart State: " << updated_cart_state << endl;
	//either timeout or already pickup all needed parts, go back local area to assemble, and complete
	fout << "Current Time: " << end_assembly_time.time_since_epoch().count() << " us" << endl;
	fout << "Updated Local State: " << final_local_state << endl;
	fout << "Updated Cart State: " << final_cart_state << endl;
	fout << "Total Completed Products: " << total_complete << endl;
	fout << endl;

	return;
}

void preFill(PartWorker& worker, Buffer& buffer) {
	//make parts(1,1,1,1,1)
	int sleeptime{ 0 };
	for (auto& i : make_part) { sleeptime += i; }
	this_thread::sleep_for(microseconds(sleeptime));
	//move parts to buffer area
	sleeptime = 0;
	for (auto& i : move_part) { sleeptime += i; }
	this_thread::sleep_for(microseconds(sleeptime));
	//load onto buffer[0]
	lock_guard<mutex> LG0(m_buf);
	for (int i = 0;i < 5;i++) {
		buffer.buf_state[0][i]++;
	}
}
