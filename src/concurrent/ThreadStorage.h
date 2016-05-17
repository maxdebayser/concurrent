#ifndef THREAD_STORAGE_H
#define THREAD_STORAGE_H

#include <exception/Exception.h>

struct ItemBase {
    virtual ~ItemBase();
};

template<typename T>
struct Item: public ItemBase {

    Item(T t): m_t(t) {}

    template<class... Args>
    Item(Args&&... args) : m_t(args...) {}

    T m_t;
};


template<typename T>
struct Item<T*>: public ItemBase {

    Item(T* t): m_t(t) {}

    ~Item() {
        if (m_t) {
            delete m_t;
            m_t = nullptr;
        }
    }

    T* m_t;
};

class ThreadStorageBase {
public:

    ThreadStorageBase();
    ~ThreadStorageBase();

    bool hasLocalData() const;

protected:
    ItemBase* get_item();
    void set_item(ItemBase* item);
};

template<class T>
class ThreadStorage: public ThreadStorageBase {
public:
    ThreadStorage() {}
    ~ThreadStorage() {}

    T& localData() {
        ItemBase* base = get_item();
        if (base == nullptr) {
            throw ExceptionLib::InvalidStateException("local data is empty");
        }
        Item<T>* item = dynamic_cast<Item<T>*>(base);
        if (item == nullptr) {
            throw ExceptionLib::ProgrammingError("local data is not of the expected type");
        }
        return item->m_t;
    }

    const T& localData() const {
        return const_cast<ThreadStorage*>(this)->localData();
    }

    void setLocalData(T data) {
        set_item(new Item<T>(data));
    }

    template<class... Args>
    void emplaceLocalData(Args&&... args) {
        set_item(new Item<T>(args...));
    }
};

void initialize_thread_storage();


#endif /* THREAD_STORAGE_H */
