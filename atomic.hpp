#include <atomic>

template <class T>
class atomic_stamped {
private:
    union __ref {
        struct { T* ptr; uint64_t stamp; } pair;
        __uint128_t val;
	};
	__ref ref;

public:
    atomic_stamped(T* ptr, uint64_t stamp) {
        set(ptr, stamp);
    }

    bool cas(T* curr, T* next, uint64_t stamp, uint64_t nstamp) {
        __ref c, n;
		c.pair.ptr = curr;
		c.pair.stamp = stamp;
		n.pair.ptr = next;
		n.pair.stamp = nstamp;
		bool res = __atomic_compare_exchange(&ref.val, &c.val, &n.val, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
		return res;
    }

    T* get(uint64_t& stamp) {
        __ref u;
		__atomic_load(&ref.val, &u.val, __ATOMIC_RELAXED);
		stamp = u.pair.stamp;
		return u.pair.ptr;
    }

    void set(T* ptr, uint64_t stamp) {
        __ref u;
		u.pair.ptr = ptr;
		u.pair.stamp = stamp;
		__atomic_store(&ref.val, &u.val, __ATOMIC_RELAXED);
    }
};
