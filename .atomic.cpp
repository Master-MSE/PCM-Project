// compiler avec:
// g++ -o atomic atomic.cpp

#include <iostream>


template <class T>
class atomic_stamped {

private:
	union __ref {
			struct { T* ptr; uint64_t stamp; } pair;
			__uint128_t val;
	};
	__ref ref;

public:

	// construct an atomic_stamped
	// with initial values for pointer and stamp

	atomic_stamped(T* ptr, uint64_t stamp)
	{
		set(ptr, stamp);
	}


	// compare and set
	// curr is the current pointer value
	// next is the new pointer value
	// stamp is the current stamp value
	// nstamp is the new stamp value

	bool cas(T* curr, T* next, uint64_t stamp, uint64_t nstamp)
	{
		__ref c, n;
		c.pair.ptr = curr;
		c.pair.stamp = stamp;
		n.pair.ptr = next;
		n.pair.stamp = nstamp;
		bool res = __atomic_compare_exchange(&ref.val, &c.val, &n.val, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
		return res;
	}

	// get pointer
	// returns the pointer
	// caller must pass an integer to receive the stamp

	T* get(uint64_t &stamp)
	{
		__ref u;
		__atomic_load(&ref.val, &u.val, __ATOMIC_RELAXED);
		stamp = u.pair.stamp;
		return u.pair.ptr;
	}

	// set pointer and stamp values

	void set(T* ptr, uint64_t stamp)
	{
		__ref u;
		u.pair.ptr = ptr;
		u.pair.stamp = stamp;
		__atomic_store(&ref.val, &u.val, __ATOMIC_RELAXED);
	}
			
};

// int main()
// {
// 	std::string sa = "a";
// 	atomic_stamped<std::string> ar(&sa, 10);

// 	uint64_t stamp;
// 	std::string* sp = ar.get(stamp);
// 	std::cout << "(start) string = " << *sp << " stamp = " << stamp << '\n';

// 	std::string sb = "b";
// 	bool c = ar.cas(&sa, &sb, 10, 12);
// 	sp = ar.get(stamp);
// 	std::cout << "(cas=" << c << ") string = " << *sp << " stamp = " << stamp << '\n';

// 	c = ar.cas(&sa, &sb, 10, 12);
// 	sp = ar.get(stamp);
// 	std::cout << "(cas=" << c << ") string = " << *sp << " stamp = " << stamp << '\n';

// 	return 0;
// }
