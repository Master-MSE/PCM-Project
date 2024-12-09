template <class T>
class atomic_stamped {
private:
    struct __ref {
        T* ptr;
        uint64_t stamp;
    };

    std::atomic<__ref> ref;

public:
    atomic_stamped(T* ptr, uint64_t stamp) {
        set(ptr, stamp);
    }

    bool cas(T* curr, T* next, uint64_t stamp, uint64_t nstamp) {
        __ref expected = {curr, stamp};
        __ref desired = {next, nstamp};
        return ref.compare_exchange_strong(expected, desired);
    }

    T* get(uint64_t& stamp) {
        __ref current = ref.load();
        stamp = current.stamp;
        return current.ptr;
    }

    void set(T* ptr, uint64_t stamp) {
        ref.store({ptr, stamp});
    }
};
